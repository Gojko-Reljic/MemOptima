#include <memoptima/sampling/jsonl_serializer.hpp>

#include <memoptima/application_info.hpp>
#include <memoptima/utc_timestamp.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <exception>
#include <string>
#include <utility>

namespace memoptima::sampling {
namespace {

using Json = nlohmann::ordered_json;

constexpr std::uint32_t kSchemaVersion = 1;
[[nodiscard]] std::string decimal(std::uint64_t value) { return std::to_string(value); }

[[nodiscard]] std::string error_code_name(CgroupMetricReadErrorCode code) {
    switch (code) {
    case CgroupMetricReadErrorCode::invalid_cgroup_path:
        return "invalid_cgroup_path";
    case CgroupMetricReadErrorCode::file_open_failed:
        return "file_open_failed";
    case CgroupMetricReadErrorCode::file_read_failed:
        return "file_read_failed";
    case CgroupMetricReadErrorCode::invalid_format:
        return "invalid_format";
    case CgroupMetricReadErrorCode::missing_required_value:
        return "missing_required_value";
    case CgroupMetricReadErrorCode::duplicate_required_value:
        return "duplicate_required_value";
    case CgroupMetricReadErrorCode::invalid_value:
        return "invalid_value";
    case CgroupMetricReadErrorCode::value_out_of_range:
        return "value_out_of_range";
    case CgroupMetricReadErrorCode::value_overflow:
        return "value_overflow";
    }
    return "unknown";
}

[[nodiscard]] Json diagnostic(const CgroupMetricReadError &error) {
    return Json{{"code", error_code_name(error.code)},
                {"source", error.source_name},
                {"detail", error.detail}};
}

[[nodiscard]] std::string warning_code_name(DockerContainerScanWarningCode code) {
    switch (code) {
    case DockerContainerScanWarningCode::docker_cgroup_layout_not_found:
        return "docker_cgroup_layout_not_found";
    case DockerContainerScanWarningCode::cgroup_directory_open_failed:
        return "cgroup_directory_open_failed";
    case DockerContainerScanWarningCode::cgroup_directory_read_failed:
        return "cgroup_directory_read_failed";
    case DockerContainerScanWarningCode::invalid_container_identifier:
        return "invalid_container_identifier";
    case DockerContainerScanWarningCode::duplicate_container_identifier:
        return "duplicate_container_identifier";
    }
    return "unknown";
}

[[nodiscard]] Json warning(const DockerContainerScanWarning &value) {
    return Json{{"code", warning_code_name(value.code)},
                {"cgroup_path", value.cgroup_path.string()},
                {"detail", value.detail}};
}

[[nodiscard]] std::string risk_name(analysis::CgroupMemoryRiskLevel risk) {
    switch (risk) {
    case analysis::CgroupMemoryRiskLevel::Low:
        return "low";
    case analysis::CgroupMemoryRiskLevel::Moderate:
        return "moderate";
    case analysis::CgroupMemoryRiskLevel::High:
        return "high";
    case analysis::CgroupMemoryRiskLevel::Critical:
        return "critical";
    }
    return "unknown";
}

[[nodiscard]] std::string coverage_name(analysis::CgroupMemoryAssessmentCoverage coverage) {
    switch (coverage) {
    case analysis::CgroupMemoryAssessmentCoverage::Complete:
        return "complete";
    case analysis::CgroupMemoryAssessmentCoverage::Partial:
        return "partial";
    }
    return "unknown";
}

[[nodiscard]] std::string finding_code_name(analysis::CgroupMemoryFindingCode code) {
    switch (code) {
    case analysis::CgroupMemoryFindingCode::memory_high_reached:
        return "memory_high_reached";
    case analysis::CgroupMemoryFindingCode::memory_max_event_recorded:
        return "memory_max_event_recorded";
    case analysis::CgroupMemoryFindingCode::memory_max_exceeded:
        return "memory_max_exceeded";
    case analysis::CgroupMemoryFindingCode::memory_max_utilization_high:
        return "memory_max_utilization_high";
    case analysis::CgroupMemoryFindingCode::memory_max_utilization_moderate:
        return "memory_max_utilization_moderate";
    case analysis::CgroupMemoryFindingCode::oom_event_recorded:
        return "oom_event_recorded";
    case analysis::CgroupMemoryFindingCode::oom_kill_recorded:
        return "oom_kill_recorded";
    }
    return "unknown";
}

[[nodiscard]] std::string evidence_code_name(analysis::CgroupMemoryEvidenceCode code) {
    switch (code) {
    case analysis::CgroupMemoryEvidenceCode::memory_current_bytes:
        return "memory_current_bytes";
    case analysis::CgroupMemoryEvidenceCode::memory_high_bytes:
        return "memory_high_bytes";
    case analysis::CgroupMemoryEvidenceCode::memory_max_bytes:
        return "memory_max_bytes";
    case analysis::CgroupMemoryEvidenceCode::memory_events_high:
        return "memory_events_high";
    case analysis::CgroupMemoryEvidenceCode::memory_events_max:
        return "memory_events_max";
    case analysis::CgroupMemoryEvidenceCode::memory_events_oom:
        return "memory_events_oom";
    case analysis::CgroupMemoryEvidenceCode::memory_events_oom_kill:
        return "memory_events_oom_kill";
    }
    return "unknown";
}

[[nodiscard]] Json assessment(const analysis::CgroupMemoryAssessment &value) {
    Json findings = Json::array();
    for (const analysis::CgroupMemoryFinding &finding : value.findings()) {
        Json evidence = Json::array();
        for (const analysis::CgroupMemoryEvidence &item : finding.evidence) {
            evidence.push_back(
                Json{{"code", evidence_code_name(item.code)}, {"value", decimal(item.value)}});
        }
        findings.push_back(Json{{"code", finding_code_name(finding.code)},
                                {"risk", risk_name(finding.risk)},
                                {"evidence", std::move(evidence)},
                                {"recommendation", finding.recommendation}});
    }

    Json unavailable_evidence = Json::array();
    for (const CgroupMetricReadError &error : value.unavailable_evidence()) {
        unavailable_evidence.push_back(diagnostic(error));
    }
    return Json{{"overall_risk", risk_name(value.overall_risk())},
                {"coverage", coverage_name(value.coverage())},
                {"findings", std::move(findings)},
                {"unavailable_evidence", std::move(unavailable_evidence)}};
}

template <typename Value>
[[nodiscard]] Json metric(const CgroupMetric<Value> &value, Json &unavailable_metrics) {
    if (value.has_value()) {
        return decimal(*value);
    }
    unavailable_metrics.push_back(diagnostic(value.error()));
    return nullptr;
}

[[nodiscard]] Json limit(const CgroupMemoryLimitMetric &value, Json &unavailable_metrics) {
    if (!value.has_value()) {
        unavailable_metrics.push_back(diagnostic(value.error()));
        return nullptr;
    }
    if (!value->has_value()) {
        return "max";
    }
    return decimal(**value);
}

[[nodiscard]] Json pressure(const CgroupMetric<CgroupPressureLine> &value,
                            Json &unavailable_metrics) {
    if (!value.has_value()) {
        unavailable_metrics.push_back(diagnostic(value.error()));
        return nullptr;
    }
    return Json{{"average_10_seconds", value->average_10_seconds},
                {"average_60_seconds", value->average_60_seconds},
                {"average_300_seconds", value->average_300_seconds},
                {"total_microseconds", decimal(value->total_microseconds)}};
}

[[nodiscard]] Json raw_memory(const CgroupMemorySnapshot &snapshot) {
    Json unavailable_metrics = Json::array();
    const CgroupMemoryLimits &limits = snapshot.limits();
    const CgroupMemoryEvents &events = snapshot.events();
    const CgroupMemoryComposition &composition = snapshot.composition();
    const CgroupMemoryPressure &memory_pressure = snapshot.pressure();
    return Json{
        {"current_bytes", metric(snapshot.current_memory(), unavailable_metrics)},
        {"high_bytes", limit(limits.high, unavailable_metrics)},
        {"max_bytes", limit(limits.maximum, unavailable_metrics)},
        {"events", Json{{"high", metric(events.high, unavailable_metrics)},
                        {"max", metric(events.maximum, unavailable_metrics)},
                        {"oom", metric(events.oom, unavailable_metrics)},
                        {"oom_kill", metric(events.oom_kill, unavailable_metrics)}}},
        {"stat", Json{{"anonymous_bytes", metric(composition.anonymous, unavailable_metrics)},
                      {"file_backed_bytes", metric(composition.file_backed, unavailable_metrics)}}},
        {"swap_current_bytes", metric(snapshot.current_swap(), unavailable_metrics)},
        {"psi", Json{{"some", pressure(memory_pressure.some, unavailable_metrics)},
                     {"full", pressure(memory_pressure.full, unavailable_metrics)}}},
        {"unavailable_metrics", std::move(unavailable_metrics)}};
}

[[nodiscard]] Json container(const DockerContainerEvidence &evidence) {
    const DockerContainerMemorySnapshot &snapshot = evidence.snapshot();
    return Json{{"container_id", snapshot.container_id().value()},
                {"cgroup_path", snapshot.cgroup_path().string()},
                {"memory", raw_memory(snapshot.memory())},
                {"assessment", assessment(evidence.assessment())}};
}

[[nodiscard]] std::string outcome_name(SamplingSessionOutcome outcome) {
    switch (outcome) {
    case SamplingSessionOutcome::completed:
        return "completed";
    case SamplingSessionOutcome::interrupted:
        return "interrupted";
    case SamplingSessionOutcome::failed:
        return "failed";
    }
    return "unknown";
}

[[nodiscard]] std::string failure_code_name(SamplingSessionFailureCode code) {
    switch (code) {
    case SamplingSessionFailureCode::output_size_limit_reached:
        return "output_size_limit_reached";
    case SamplingSessionFailureCode::output_write_failed:
        return "output_write_failed";
    case SamplingSessionFailureCode::output_flush_failed:
        return "output_flush_failed";
    }
    return "unknown";
}

template <typename BuildRecord>
[[nodiscard]] std::expected<std::string, SamplingJsonlSerializationError>
serialize_record(std::chrono::system_clock::time_point timestamp, BuildRecord build_record) {
    const auto formatted_timestamp =
        format_utc_timestamp(timestamp, UtcTimestampPrecision::milliseconds);
    if (!formatted_timestamp.has_value()) {
        return std::unexpected(SamplingJsonlSerializationError{
            .code = SamplingJsonlSerializationErrorCode::timestamp_format_failed,
            .detail = formatted_timestamp.error().detail,
        });
    }

    try {
        return build_record(*formatted_timestamp).dump();
    } catch (const std::exception &exception) {
        return std::unexpected(SamplingJsonlSerializationError{
            .code = SamplingJsonlSerializationErrorCode::json_serialization_failed,
            .detail = exception.what(),
        });
    }
}

} // namespace

std::expected<std::string, SamplingJsonlSerializationError>
serialize_session_start(SamplingCount requested_count, SamplingInterval requested_interval,
                        std::chrono::system_clock::time_point started_at) {
    return serialize_record(
        started_at, [requested_count, requested_interval](const std::string &time) {
            return Json{{"record_type", "session_start"},
                        {"schema_version", kSchemaVersion},
                        {"application_version", version()},
                        {"started_at_utc", time},
                        {"requested_sample_count", requested_count.value()},
                        {"requested_interval_ms", decimal(requested_interval.value().count())},
                        {"output_max_bytes", decimal(kSamplingOutputMaximumBytes)},
                        {"output_permissions_octal", "0600"},
                        {"flush_after_each_record", true},
                        {"host_identifiers_included", false}};
        });
}

std::expected<std::string, SamplingJsonlSerializationError>
serialize_sample(const SamplingEvidenceSample &sample) {
    return serialize_record(sample.captured_at(), [&sample](const std::string &time) {
        Json warnings = Json::array();
        for (const DockerContainerScanWarning &item : sample.discovery_warnings()) {
            warnings.push_back(warning(item));
        }
        Json containers = Json::array();
        for (const DockerContainerEvidence &item : sample.containers()) {
            containers.push_back(container(item));
        }
        return Json{{"record_type", "sample"},
                    {"schema_version", kSchemaVersion},
                    {"sample_sequence", sample.sequence().value()},
                    {"planned_offset_ms", decimal(sample.planned_offset().count())},
                    {"captured_at_utc", time},
                    {"schedule_delay_ms", decimal(sample.schedule_delay().count())},
                    {"skipped_container_count", decimal(sample.skipped_container_count())},
                    {"discovery_warnings", std::move(warnings)},
                    {"containers", std::move(containers)}};
    });
}

std::expected<std::string, SamplingJsonlSerializationError>
serialize_session_end(SamplingSessionOutcome outcome, std::uint32_t completed_sample_count,
                      std::chrono::system_clock::time_point ended_at,
                      const std::optional<SamplingSessionFailure> &failure) {
    const bool failure_is_required = outcome == SamplingSessionOutcome::failed;
    if (failure_is_required != failure.has_value()) {
        return std::unexpected(SamplingJsonlSerializationError{
            .code = SamplingJsonlSerializationErrorCode::invalid_session_end,
            .detail =
                "A failed session requires failure details; other outcomes must not have them.",
        });
    }
    return serialize_record(
        ended_at, [outcome, completed_sample_count, &failure](const std::string &time) {
            Json failure_value = nullptr;
            if (failure.has_value()) {
                failure_value =
                    Json{{"code", failure_code_name(failure->code)}, {"detail", failure->detail}};
            }
            return Json{{"record_type", "session_end"},
                        {"schema_version", kSchemaVersion},
                        {"outcome", outcome_name(outcome)},
                        {"completed_sample_count", completed_sample_count},
                        {"ended_at_utc", time},
                        {"failure", std::move(failure_value)}};
        });
}

} // namespace memoptima::sampling
