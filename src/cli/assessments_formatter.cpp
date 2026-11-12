#include "cli/assessments_formatter.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr std::uint64_t kBytesPerUnit = 1024;
constexpr std::array<std::string_view, 5> kUnitNames{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr int kContainerIdColumnWidth = 66;
constexpr int kRiskColumnWidth = 10;
constexpr int kCoverageColumnWidth = 11;
constexpr int kTopFindingColumnWidth = 39;

[[nodiscard]] std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0) {
        return "0 B";
    }

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};
    while (display_value >= static_cast<double>(kBytesPerUnit) &&
           unit_index + 1 < kUnitNames.size()) {
        display_value /= static_cast<double>(kBytesPerUnit);
        ++unit_index;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << display_value << ' ' << kUnitNames[unit_index];
    return output.str();
}

[[nodiscard]] std::string_view risk_level_name(analysis::CgroupMemoryRiskLevel risk) noexcept {
    switch (risk) {
    case analysis::CgroupMemoryRiskLevel::Low:
        return "Low";
    case analysis::CgroupMemoryRiskLevel::Moderate:
        return "Moderate";
    case analysis::CgroupMemoryRiskLevel::High:
        return "High";
    case analysis::CgroupMemoryRiskLevel::Critical:
        return "Critical";
    }

    return "Critical";
}

[[nodiscard]] std::string_view
coverage_name(analysis::CgroupMemoryAssessmentCoverage coverage) noexcept {
    switch (coverage) {
    case analysis::CgroupMemoryAssessmentCoverage::Complete:
        return "Complete";
    case analysis::CgroupMemoryAssessmentCoverage::Partial:
        return "Partial";
    }

    return "Partial";
}

[[nodiscard]] std::string_view finding_code_name(analysis::CgroupMemoryFindingCode code) noexcept {
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

    return "oom_kill_recorded";
}

[[nodiscard]] std::string_view
evidence_code_name(analysis::CgroupMemoryEvidenceCode code) noexcept {
    switch (code) {
    case analysis::CgroupMemoryEvidenceCode::memory_current_bytes:
        return "memory.current";
    case analysis::CgroupMemoryEvidenceCode::memory_high_bytes:
        return "memory.high";
    case analysis::CgroupMemoryEvidenceCode::memory_max_bytes:
        return "memory.max";
    case analysis::CgroupMemoryEvidenceCode::memory_events_high:
        return "memory.events.high";
    case analysis::CgroupMemoryEvidenceCode::memory_events_max:
        return "memory.events.max";
    case analysis::CgroupMemoryEvidenceCode::memory_events_oom:
        return "memory.events.oom";
    case analysis::CgroupMemoryEvidenceCode::memory_events_oom_kill:
        return "memory.events.oom_kill";
    }

    return "memory.events.oom_kill";
}

[[nodiscard]] bool is_byte_evidence(analysis::CgroupMemoryEvidenceCode code) noexcept {
    return code == analysis::CgroupMemoryEvidenceCode::memory_current_bytes ||
           code == analysis::CgroupMemoryEvidenceCode::memory_high_bytes ||
           code == analysis::CgroupMemoryEvidenceCode::memory_max_bytes;
}

[[nodiscard]] std::string format_evidence_value(const analysis::CgroupMemoryEvidence &evidence) {
    return is_byte_evidence(evidence.code) ? format_bytes(evidence.value)
                                           : std::to_string(evidence.value);
}

} // namespace

std::string
format_docker_container_assessment_scan_result(const DockerContainerAssessmentScanResult &result) {
    if (result.containers().empty()) {
        return "No supported rootful Docker containers found.\n";
    }

    std::ostringstream output;
    output << "Docker container assessments\n\n"
           << std::left << std::setw(kContainerIdColumnWidth) << "CONTAINER ID"
           << std::setw(kRiskColumnWidth) << "RISK" << std::setw(kCoverageColumnWidth) << "COVERAGE"
           << std::setw(kTopFindingColumnWidth) << "TOP FINDING"
           << "FINDINGS\n";
    for (const DockerContainerAssessment &container : result.containers()) {
        const analysis::CgroupMemoryAssessment &assessment = container.assessment();
        const std::string_view top_finding =
            assessment.findings().empty() ? std::string_view{"None"}
                                          : finding_code_name(assessment.findings().front().code);
        output << std::left << std::setw(kContainerIdColumnWidth)
               << container.container_id().value() << std::setw(kRiskColumnWidth)
               << risk_level_name(assessment.overall_risk()) << std::setw(kCoverageColumnWidth)
               << coverage_name(assessment.coverage()) << std::setw(kTopFindingColumnWidth)
               << top_finding << assessment.findings().size() << '\n';
    }
    return output.str();
}

std::string format_docker_container_assessment_detail(const DockerContainerAssessment &container) {
    const analysis::CgroupMemoryAssessment &assessment = container.assessment();
    std::ostringstream output;
    output << "Container\n"
           << "  ID:                  " << container.container_id().value() << '\n'
           << "  Cgroup path:         " << container.cgroup_path().string() << "\n\n"
           << "Overall risk\n"
           << "  Level:               " << risk_level_name(assessment.overall_risk()) << "\n\n"
           << "Coverage\n"
           << "  Status:              " << coverage_name(assessment.coverage()) << "\n\n"
           << "Findings\n";
    if (assessment.findings().empty()) {
        output << "  No findings were produced from the currently available boundary and OOM "
                  "signals.\n";
    } else {
        std::size_t number = 1;
        for (const analysis::CgroupMemoryFinding &finding : assessment.findings()) {
            output << "  " << number << ". " << finding_code_name(finding.code) << "\n"
                   << "     Risk:             " << risk_level_name(finding.risk) << "\n"
                   << "     Evidence:\n";
            for (const analysis::CgroupMemoryEvidence &evidence : finding.evidence) {
                output << "       " << std::left << std::setw(24)
                       << evidence_code_name(evidence.code) << format_evidence_value(evidence)
                       << '\n';
            }
            output << "     Recommendation:   " << finding.recommendation << '\n';
            ++number;
        }
    }

    if (!assessment.unavailable_evidence().empty()) {
        output << "\nUnavailable evidence\n";
        for (const CgroupMetricReadError &error : assessment.unavailable_evidence()) {
            output << "  " << error.source_name << ": " << error.detail << '\n';
        }
    }
    return output.str();
}

} // namespace memoptima::cli
