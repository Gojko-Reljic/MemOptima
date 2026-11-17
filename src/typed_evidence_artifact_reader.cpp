#include "typed_evidence_artifact_reader.hpp"

#include "utc_timestamp_parser.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <string>
#include <string_view>
#include <utility>

namespace memoptima::detail {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] std::unexpected<EvidenceArtifactTraversalError> typed_error(std::uint64_t line_number,
                                                                          std::string detail) {
    return std::unexpected(EvidenceArtifactTraversalError{
        .detail = "Line " + std::to_string(line_number) + ": " + std::move(detail),
    });
}

[[nodiscard]] std::expected<std::uint64_t, EvidenceArtifactTraversalError>
parse_decimal(const Json &value, std::uint64_t line_number, std::string_view field_name) {
    const std::string text = value.get<std::string>();
    std::uint64_t parsed{};
    const auto [position, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || position != text.data() + text.size()) {
        return typed_error(line_number, std::string{field_name} + " cannot be converted safely.");
    }
    return parsed;
}

[[nodiscard]] std::expected<std::chrono::sys_time<std::chrono::milliseconds>,
                            EvidenceArtifactTraversalError>
parse_timestamp(const Json &value, std::uint64_t line_number, std::string_view field_name) {
    const auto parsed = parse_canonical_utc_timestamp(value.get<std::string>());
    if (!parsed.has_value()) {
        return typed_error(line_number,
                           std::string{field_name} + " is invalid: " + parsed.error().detail);
    }
    return *parsed;
}

[[nodiscard]] std::expected<TypedEvidenceMemoryMaximum, EvidenceArtifactTraversalError>
parse_maximum_memory(const Json &value, std::uint64_t line_number) {
    if (value.is_null()) {
        return TypedEvidenceMemoryMaximum{.kind = TypedEvidenceMemoryMaximumKind::unavailable};
    }
    if (value == "max") {
        return TypedEvidenceMemoryMaximum{.kind = TypedEvidenceMemoryMaximumKind::unlimited};
    }
    const auto bytes = parse_decimal(value, line_number, "memory.max_bytes");
    if (!bytes.has_value()) {
        return std::unexpected(bytes.error());
    }
    return TypedEvidenceMemoryMaximum{.kind = TypedEvidenceMemoryMaximumKind::finite,
                                      .bytes = *bytes};
}

[[nodiscard]] std::expected<TypedEvidenceContainerObservation, EvidenceArtifactTraversalError>
parse_container(const Json &value, std::uint64_t line_number) {
    const auto container_id = DockerContainerId::parse(value.at("container_id").get<std::string>());
    if (!container_id.has_value()) {
        return typed_error(line_number, "container_id is invalid: " + container_id.error().detail);
    }

    const auto &memory = value.at("memory");
    std::optional<std::uint64_t> current_memory_bytes;
    if (!memory.at("current_bytes").is_null()) {
        const auto parsed =
            parse_decimal(memory.at("current_bytes"), line_number, "memory.current_bytes");
        if (!parsed.has_value()) {
            return std::unexpected(parsed.error());
        }
        current_memory_bytes = *parsed;
    }

    const auto maximum_memory = parse_maximum_memory(memory.at("max_bytes"), line_number);
    if (!maximum_memory.has_value()) {
        return std::unexpected(maximum_memory.error());
    }
    return TypedEvidenceContainerObservation{
        .container_id = *container_id,
        .cgroup_path = value.at("cgroup_path").get<std::string>(),
        .current_memory_bytes = current_memory_bytes,
        .maximum_memory = *maximum_memory,
    };
}

[[nodiscard]] std::expected<sampling::SamplingSessionOutcome, EvidenceArtifactTraversalError>
parse_outcome(const Json &value, std::uint64_t line_number) {
    const std::string outcome = value.get<std::string>();
    if (outcome == "completed") {
        return sampling::SamplingSessionOutcome::completed;
    }
    if (outcome == "interrupted") {
        return sampling::SamplingSessionOutcome::interrupted;
    }
    if (outcome == "failed") {
        return sampling::SamplingSessionOutcome::failed;
    }
    return typed_error(line_number, "session outcome is invalid.");
}

class TypedRecordAdapter final : public EvidenceArtifactRecordSink {
  public:
    explicit TypedRecordAdapter(TypedEvidenceArtifactSink &sink) : sink_{sink} {}

    std::expected<void, EvidenceArtifactTraversalError>
    consume(const EvidenceArtifactRecord &record) override {
        const Json value = Json::parse(record.canonical_json);
        if (record.type == EvidenceRecordType::session_start) {
            const auto started_at =
                parse_timestamp(value.at("started_at_utc"), record.line_number, "started_at_utc");
            const auto interval = parse_decimal(value.at("requested_interval_ms"),
                                                record.line_number, "requested_interval_ms");
            if (!started_at.has_value()) {
                return std::unexpected(started_at.error());
            }
            if (!interval.has_value()) {
                return std::unexpected(interval.error());
            }
            return sink_.consume(TypedEvidenceSessionStart{
                .line_number = record.line_number,
                .started_at = *started_at,
                .requested_sample_count = value.at("requested_sample_count").get<std::uint32_t>(),
                .requested_interval_ms = *interval,
            });
        }
        if (record.type == EvidenceRecordType::sample) {
            const auto captured_at =
                parse_timestamp(value.at("captured_at_utc"), record.line_number, "captured_at_utc");
            if (!captured_at.has_value()) {
                return std::unexpected(captured_at.error());
            }
            TypedEvidenceSample sample{
                .line_number = record.line_number, .captured_at = *captured_at, .containers = {}};
            for (const auto &container : value.at("containers")) {
                const auto observation = parse_container(container, record.line_number);
                if (!observation.has_value()) {
                    return std::unexpected(observation.error());
                }
                sample.containers.push_back(*observation);
            }
            return sink_.consume(sample);
        }

        const auto ended_at =
            parse_timestamp(value.at("ended_at_utc"), record.line_number, "ended_at_utc");
        const auto outcome = parse_outcome(value.at("outcome"), record.line_number);
        if (!ended_at.has_value()) {
            return std::unexpected(ended_at.error());
        }
        if (!outcome.has_value()) {
            return std::unexpected(outcome.error());
        }
        return sink_.consume(TypedEvidenceSessionEnd{
            .line_number = record.line_number,
            .outcome = *outcome,
            .ended_at = *ended_at,
        });
    }

  private:
    TypedEvidenceArtifactSink &sink_;
};

} // namespace

std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_typed_evidence_artifact(const std::filesystem::path &path,
                                 TypedEvidenceArtifactSink &sink) {
    TypedRecordAdapter adapter{sink};
    return traverse_evidence_artifact(path, adapter);
}

} // namespace memoptima::detail
