#pragma once

#include <memoptima/cgroup_memory_snapshot.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::analysis {

enum class CgroupMemoryRiskLevel : std::uint8_t {
    Low,
    Moderate,
    High,
    Critical,
};

enum class CgroupMemoryAssessmentCoverage : std::uint8_t {
    Complete,
    Partial,
};

enum class CgroupMemoryFindingCode : std::uint8_t {
    memory_high_reached,
    memory_max_event_recorded,
    memory_max_exceeded,
    memory_max_utilization_high,
    memory_max_utilization_moderate,
    oom_event_recorded,
    oom_kill_recorded,
};

enum class CgroupMemoryEvidenceCode : std::uint8_t {
    memory_current_bytes,
    memory_high_bytes,
    memory_max_bytes,
    memory_events_high,
    memory_events_max,
    memory_events_oom,
    memory_events_oom_kill,
};

struct CgroupMemoryEvidence {
    CgroupMemoryEvidenceCode code;
    std::uint64_t value;
};

struct CgroupMemoryFinding {
    CgroupMemoryFindingCode code;
    CgroupMemoryRiskLevel risk;
    std::vector<CgroupMemoryEvidence> evidence;
    std::string recommendation;
};

class CgroupMemoryAssessment {
  public:
    [[nodiscard]] static CgroupMemoryAssessment
    create(CgroupMemoryRiskLevel overall_risk, CgroupMemoryAssessmentCoverage coverage,
           std::vector<CgroupMemoryFinding> findings,
           std::vector<CgroupMetricReadError> unavailable_evidence);

    [[nodiscard]] CgroupMemoryRiskLevel overall_risk() const noexcept;
    [[nodiscard]] CgroupMemoryAssessmentCoverage coverage() const noexcept;
    [[nodiscard]] const std::vector<CgroupMemoryFinding> &findings() const noexcept;
    [[nodiscard]] const std::vector<CgroupMetricReadError> &unavailable_evidence() const noexcept;

  private:
    CgroupMemoryAssessment(CgroupMemoryRiskLevel overall_risk,
                           CgroupMemoryAssessmentCoverage coverage,
                           std::vector<CgroupMemoryFinding> findings,
                           std::vector<CgroupMetricReadError> unavailable_evidence)
        : overall_risk_{overall_risk}, coverage_{coverage}, findings_{std::move(findings)},
          unavailable_evidence_{std::move(unavailable_evidence)} {}

    CgroupMemoryRiskLevel overall_risk_;
    CgroupMemoryAssessmentCoverage coverage_;
    std::vector<CgroupMemoryFinding> findings_;
    std::vector<CgroupMetricReadError> unavailable_evidence_;
};

[[nodiscard]] CgroupMemoryAssessment
assess_cgroup_memory_snapshot(const CgroupMemorySnapshot &snapshot);

} // namespace memoptima::analysis
