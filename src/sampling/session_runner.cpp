#include <memoptima/sampling/session_runner.hpp>

#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/jsonl_writer.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::sampling {
namespace {

constexpr auto kStopCheckInterval = std::chrono::milliseconds{100};

[[nodiscard]] SamplingSessionRunError make_error(SamplingSessionRunErrorCode code,
                                                 std::string detail) {
    return SamplingSessionRunError{.code = code, .detail = std::move(detail)};
}

[[nodiscard]] SamplingSessionRunError map_writer_error(const SamplingJsonlWriterError &error) {
    const SamplingSessionRunErrorCode code =
        error.code == SamplingJsonlWriterErrorCode::output_flush_failed
            ? SamplingSessionRunErrorCode::output_flush_failed
        : error.code == SamplingJsonlWriterErrorCode::output_write_failed
            ? SamplingSessionRunErrorCode::output_write_failed
            : SamplingSessionRunErrorCode::output_creation_failed;
    return make_error(code, error.detail);
}

[[nodiscard]] SamplingSessionRunError
map_serialization_error(const SamplingJsonlSerializationError &error) {
    return make_error(SamplingSessionRunErrorCode::serialization_failed, error.detail);
}

void wait_until(const SamplingSessionDependencies &dependencies,
                std::chrono::steady_clock::time_point deadline) {
    while (!dependencies.stop_requested()) {
        const auto remaining = deadline - dependencies.current_steady_time();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return;
        }
        const auto remaining_milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
        if (remaining_milliseconds <= std::chrono::milliseconds::zero()) {
            return;
        }
        dependencies.sleep_for(std::min(kStopCheckInterval, remaining_milliseconds));
    }
}

[[nodiscard]] std::expected<std::string, SamplingSessionRunError>
make_terminal_record(SamplingSessionOutcome outcome, std::uint32_t completed_sample_count,
                     std::chrono::system_clock::time_point timestamp,
                     const std::optional<SamplingSessionFailure> &failure) {
    const auto serialized =
        serialize_session_end(outcome, completed_sample_count, timestamp, failure);
    if (!serialized.has_value()) {
        return std::unexpected(map_serialization_error(serialized.error()));
    }
    return *serialized;
}

[[nodiscard]] std::expected<SamplingSessionResult, SamplingSessionRunError>
write_terminal_and_close(SamplingJsonlWriter &writer, SamplingSessionOutcome outcome,
                         std::uint32_t completed_sample_count,
                         std::chrono::system_clock::time_point timestamp,
                         const std::optional<SamplingSessionFailure> &failure) {
    const auto terminal_record =
        make_terminal_record(outcome, completed_sample_count, timestamp, failure);
    if (!terminal_record.has_value()) {
        return std::unexpected(terminal_record.error());
    }
    const auto write_result = writer.write_record(*terminal_record);
    if (!write_result.has_value()) {
        return std::unexpected(map_writer_error(write_result.error()));
    }
    const auto close_result = writer.close();
    if (!close_result.has_value()) {
        return std::unexpected(map_writer_error(close_result.error()));
    }
    return SamplingSessionResult{
        .outcome = outcome,
        .completed_sample_count = completed_sample_count,
        .failure = failure,
    };
}

[[nodiscard]] std::expected<std::string, SamplingSessionRunError>
make_size_limit_terminal_record(std::uint32_t completed_sample_count,
                                std::chrono::system_clock::time_point timestamp) {
    return make_terminal_record(
        SamplingSessionOutcome::failed, completed_sample_count, timestamp,
        SamplingSessionFailure{
            .code = SamplingSessionFailureCode::output_size_limit_reached,
            .detail = "The output artifact reached its configured size limit.",
        });
}

} // namespace

std::expected<SamplingSessionResult, SamplingSessionRunError>
run_sampling_session(const SamplingSessionConfiguration &configuration,
                     const SamplingSessionDependencies &dependencies) {
    const auto preflight = dependencies.preflight();
    if (!preflight.has_value()) {
        return std::unexpected(
            make_error(SamplingSessionRunErrorCode::preflight_failed, preflight.error()));
    }

    auto writer = SamplingJsonlWriter::create(configuration.output_path);
    if (!writer.has_value()) {
        return std::unexpected(map_writer_error(writer.error()));
    }

    const auto session_started_at = dependencies.current_utc_time();
    const auto session_started_steady = dependencies.current_steady_time();
    const auto session_start_record =
        serialize_session_start(configuration.count, configuration.interval, session_started_at);
    if (!session_start_record.has_value()) {
        return std::unexpected(map_serialization_error(session_start_record.error()));
    }

    const auto initial_terminal_record = make_size_limit_terminal_record(0, session_started_at);
    if (!initial_terminal_record.has_value()) {
        return std::unexpected(initial_terminal_record.error());
    }
    const std::uint64_t initial_terminal_size =
        static_cast<std::uint64_t>(initial_terminal_record->size()) + 1;
    const auto write_start = writer->write_record(*session_start_record, initial_terminal_size);
    if (!write_start.has_value()) {
        return std::unexpected(map_writer_error(write_start.error()));
    }

    std::uint32_t completed_sample_count{};
    while (completed_sample_count < configuration.count.value()) {
        const auto planned_offset = configuration.interval.value() * completed_sample_count;
        const auto deadline = session_started_steady + planned_offset;
        wait_until(dependencies, deadline);
        if (dependencies.stop_requested()) {
            return write_terminal_and_close(*writer, SamplingSessionOutcome::interrupted,
                                            completed_sample_count, dependencies.current_utc_time(),
                                            std::nullopt);
        }

        const auto collection_started_steady = dependencies.current_steady_time();
        const auto delay_duration = collection_started_steady > deadline
                                        ? collection_started_steady - deadline
                                        : std::chrono::steady_clock::duration::zero();
        const auto schedule_delay =
            std::chrono::duration_cast<std::chrono::milliseconds>(delay_duration);

        DockerContainerScanResult scan_result = dependencies.scan_containers();
        std::vector<DockerContainerEvidence> containers;
        containers.reserve(scan_result.containers().size());
        for (const DockerContainerMemorySnapshot &snapshot : scan_result.containers()) {
            containers.push_back(DockerContainerEvidence::create(snapshot));
        }

        const auto sequence =
            SamplingCount::create(static_cast<std::uint64_t>(completed_sample_count) + 1);
        if (!sequence.has_value()) {
            return std::unexpected(make_error(SamplingSessionRunErrorCode::serialization_failed,
                                              "Unable to create a valid sample sequence."));
        }
        const SamplingEvidenceSample sample = SamplingEvidenceSample::create(
            *sequence, std::chrono::duration_cast<std::chrono::milliseconds>(planned_offset),
            dependencies.current_utc_time(), schedule_delay, std::move(containers),
            scan_result.skipped_container_count(), scan_result.warnings());
        const auto sample_record = serialize_sample(sample);
        if (!sample_record.has_value()) {
            return std::unexpected(map_serialization_error(sample_record.error()));
        }

        const auto size_limit_terminal = make_size_limit_terminal_record(
            completed_sample_count, dependencies.current_utc_time());
        if (!size_limit_terminal.has_value()) {
            return std::unexpected(size_limit_terminal.error());
        }
        const std::uint64_t terminal_reservation =
            static_cast<std::uint64_t>(size_limit_terminal->size()) + 1;
        const auto write_sample = writer->write_record(*sample_record, terminal_reservation);
        if (!write_sample.has_value()) {
            if (write_sample.error().code ==
                SamplingJsonlWriterErrorCode::output_size_limit_reached) {
                return write_terminal_and_close(
                    *writer, SamplingSessionOutcome::failed, completed_sample_count,
                    dependencies.current_utc_time(),
                    SamplingSessionFailure{
                        .code = SamplingSessionFailureCode::output_size_limit_reached,
                        .detail = "The output artifact reached its configured size limit.",
                    });
            }
            return std::unexpected(map_writer_error(write_sample.error()));
        }
        ++completed_sample_count;
    }

    return write_terminal_and_close(*writer, SamplingSessionOutcome::completed,
                                    completed_sample_count, dependencies.current_utc_time(),
                                    std::nullopt);
}

} // namespace memoptima::sampling
