#include <memoptima/analysis/cgroup_memory_assessment.hpp>

#include <algorithm>
#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace memoptima::analysis {
namespace {

constexpr std::uint64_t kModerateMaximumUtilizationPercent = 90;
constexpr std::uint64_t kHighMaximumUtilizationPercent = 98;
constexpr std::uint64_t kPercentDivisor = 100;

[[nodiscard]] bool has_at_least_percent(std::uint64_t value, std::uint64_t total,
                                        std::uint64_t percent) noexcept {
    // Decomposing the total avoids overflowing a valid byte value by multiplying it by percent.
    const std::uint64_t whole_percent = total / kPercentDivisor;
    const std::uint64_t remainder = total % kPercentDivisor;
    const std::uint64_t rounded_down =
        whole_percent * percent + remainder * percent / kPercentDivisor;
    const bool has_fraction = remainder * percent % kPercentDivisor != 0;
    const std::uint64_t threshold = rounded_down + (has_fraction ? 1U : 0U);
    return value >= threshold;
}

[[nodiscard]] bool is_higher_risk(CgroupMemoryRiskLevel left,
                                  CgroupMemoryRiskLevel right) noexcept {
    return static_cast<std::uint8_t>(left) > static_cast<std::uint8_t>(right);
}

[[nodiscard]] bool finding_precedes(const CgroupMemoryFinding &left,
                                    const CgroupMemoryFinding &right) noexcept {
    if (left.risk != right.risk) {
        return is_higher_risk(left.risk, right.risk);
    }
    return static_cast<std::uint8_t>(left.code) < static_cast<std::uint8_t>(right.code);
}

void add_finding(std::vector<CgroupMemoryFinding> &findings, CgroupMemoryFindingCode code,
                 CgroupMemoryRiskLevel risk, std::vector<CgroupMemoryEvidence> evidence,
                 std::string recommendation) {
    findings.push_back({.code = code,
                        .risk = risk,
                        .evidence = std::move(evidence),
                        .recommendation = std::move(recommendation)});
}

void add_unavailable_error(std::vector<CgroupMetricReadError> &errors,
                           const CgroupMetricReadError &candidate) {
    const auto existing =
        std::ranges::find_if(errors, [&candidate](const CgroupMetricReadError &error) {
            return std::tie(error.code, error.source_name, error.detail) ==
                   std::tie(candidate.code, candidate.source_name, candidate.detail);
        });
    if (existing == errors.end()) {
        errors.push_back(candidate);
    }
}

template <typename Value>
void collect_unavailable_error(const CgroupMetric<Value> &metric,
                               std::vector<CgroupMetricReadError> &errors) {
    if (!metric.has_value()) {
        add_unavailable_error(errors, metric.error());
    }
}

} // namespace

CgroupMemoryAssessment
CgroupMemoryAssessment::create(CgroupMemoryRiskLevel overall_risk,
                               CgroupMemoryAssessmentCoverage coverage,
                               std::vector<CgroupMemoryFinding> findings,
                               std::vector<CgroupMetricReadError> unavailable_evidence) {
    return CgroupMemoryAssessment{overall_risk, coverage, std::move(findings),
                                  std::move(unavailable_evidence)};
}

CgroupMemoryRiskLevel CgroupMemoryAssessment::overall_risk() const noexcept {
    return overall_risk_;
}

CgroupMemoryAssessmentCoverage CgroupMemoryAssessment::coverage() const noexcept {
    return coverage_;
}

const std::vector<CgroupMemoryFinding> &CgroupMemoryAssessment::findings() const noexcept {
    return findings_;
}

const std::vector<CgroupMetricReadError> &
CgroupMemoryAssessment::unavailable_evidence() const noexcept {
    return unavailable_evidence_;
}

CgroupMemoryAssessment assess_cgroup_memory_snapshot(const CgroupMemorySnapshot &snapshot) {
    const CgroupByteMetric &current = snapshot.current_memory;
    const CgroupMemoryLimitMetric &high = snapshot.limits.high;
    const CgroupMemoryLimitMetric &maximum = snapshot.limits.maximum;
    const CgroupEventMetric &high_events = snapshot.events.high;
    const CgroupEventMetric &maximum_events = snapshot.events.maximum;
    const CgroupEventMetric &oom_events = snapshot.events.oom;
    const CgroupEventMetric &oom_kill_events = snapshot.events.oom_kill;

    std::vector<CgroupMetricReadError> unavailable_evidence;
    collect_unavailable_error(current, unavailable_evidence);
    collect_unavailable_error(high, unavailable_evidence);
    collect_unavailable_error(maximum, unavailable_evidence);
    collect_unavailable_error(high_events, unavailable_evidence);
    collect_unavailable_error(maximum_events, unavailable_evidence);
    collect_unavailable_error(oom_events, unavailable_evidence);
    collect_unavailable_error(oom_kill_events, unavailable_evidence);

    std::vector<CgroupMemoryFinding> findings;
    if (current.has_value() && maximum.has_value() && maximum->has_value()) {
        const std::uint64_t current_bytes = *current;
        const std::uint64_t maximum_bytes = **maximum;
        const std::vector<CgroupMemoryEvidence> evidence{
            {.code = CgroupMemoryEvidenceCode::memory_current_bytes, .value = current_bytes},
            {.code = CgroupMemoryEvidenceCode::memory_max_bytes, .value = maximum_bytes},
        };
        if (current_bytes > maximum_bytes) {
            add_finding(
                findings, CgroupMemoryFindingCode::memory_max_exceeded,
                CgroupMemoryRiskLevel::Critical, evidence,
                "Treat this as incident data; inspect the workload and cgroup configuration before "
                "applying any change.");
        } else if (has_at_least_percent(current_bytes, maximum_bytes,
                                        kHighMaximumUtilizationPercent)) {
            add_finding(findings, CgroupMemoryFindingCode::memory_max_utilization_high,
                        CgroupMemoryRiskLevel::High, evidence,
                        "Review workload demand and the configured memory.max before a cgroup OOM "
                        "condition "
                        "occurs.");
        } else if (has_at_least_percent(current_bytes, maximum_bytes,
                                        kModerateMaximumUtilizationPercent)) {
            add_finding(
                findings, CgroupMemoryFindingCode::memory_max_utilization_moderate,
                CgroupMemoryRiskLevel::Moderate, evidence,
                "Review workload demand and the configured memory.max before hard-limit pressure "
                "develops.");
        }
    }

    if (high.has_value() && high->has_value()) {
        const std::uint64_t high_bytes = **high;
        std::vector<CgroupMemoryEvidence> evidence{
            {.code = CgroupMemoryEvidenceCode::memory_high_bytes, .value = high_bytes},
        };
        bool reached{};
        if (current.has_value() && *current >= high_bytes) {
            reached = true;
            evidence.push_back(
                {.code = CgroupMemoryEvidenceCode::memory_current_bytes, .value = *current});
        }
        if (high_events.has_value() && *high_events > 0) {
            reached = true;
            evidence.push_back(
                {.code = CgroupMemoryEvidenceCode::memory_events_high, .value = *high_events});
        }
        if (reached) {
            add_finding(findings, CgroupMemoryFindingCode::memory_high_reached,
                        CgroupMemoryRiskLevel::Moderate, std::move(evidence),
                        "Review memory.high and the workload working set before pressure reaches "
                        "memory.max.");
        }
    }

    if (maximum_events.has_value() && *maximum_events > 0) {
        add_finding(
            findings, CgroupMemoryFindingCode::memory_max_event_recorded,
            CgroupMemoryRiskLevel::High,
            {{.code = CgroupMemoryEvidenceCode::memory_events_max, .value = *maximum_events}},
            "Investigate hard-limit pressure and review memory.max with the workload's memory "
            "demand.");
    }
    if (oom_events.has_value() && *oom_events > 0) {
        add_finding(findings, CgroupMemoryFindingCode::oom_event_recorded,
                    CgroupMemoryRiskLevel::High,
                    {{.code = CgroupMemoryEvidenceCode::memory_events_oom, .value = *oom_events}},
                    "Investigate cgroup OOM conditions and application logs before retrying or "
                    "resizing the "
                    "workload.");
    }
    if (oom_kill_events.has_value() && *oom_kill_events > 0) {
        add_finding(
            findings, CgroupMemoryFindingCode::oom_kill_recorded, CgroupMemoryRiskLevel::Critical,
            {{.code = CgroupMemoryEvidenceCode::memory_events_oom_kill, .value = *oom_kill_events}},
            "Treat this as a workload incident; inspect process recovery and cgroup memory "
            "settings.");
    }

    std::ranges::sort(findings, finding_precedes);
    CgroupMemoryRiskLevel overall_risk = CgroupMemoryRiskLevel::Low;
    if (!findings.empty()) {
        overall_risk = findings.front().risk;
    }
    const CgroupMemoryAssessmentCoverage coverage = unavailable_evidence.empty()
                                                        ? CgroupMemoryAssessmentCoverage::Complete
                                                        : CgroupMemoryAssessmentCoverage::Partial;
    return CgroupMemoryAssessment::create(overall_risk, coverage, std::move(findings),
                                          std::move(unavailable_evidence));
}

} // namespace memoptima::analysis
