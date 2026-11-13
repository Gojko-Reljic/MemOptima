#include "cli/assessments_formatter.hpp"

#include <gtest/gtest.h>

#include <string>
#include <utility>
#include <vector>

namespace memoptima::cli {
namespace {

[[nodiscard]] DockerContainerAssessment
make_assessment(std::string id, analysis::CgroupMemoryRiskLevel risk,
                analysis::CgroupMemoryAssessmentCoverage coverage,
                std::vector<analysis::CgroupMemoryFinding> findings = {},
                std::vector<CgroupMetricReadError> unavailable_evidence = {}) {
    const auto parsed_id = DockerContainerId::parse(id);
    EXPECT_TRUE(parsed_id.has_value());
    return {
        .container_id = *parsed_id,
        .cgroup_path = "/sys/fs/cgroup/docker/" + id,
        .assessment = analysis::CgroupMemoryAssessment::create(risk, coverage, std::move(findings),
                                                               std::move(unavailable_evidence)),
    };
}

TEST(AssessmentsFormatter, FormatsSummaryTableAndLowRiskWithoutFindings) {
    const std::string id(64, 'a');
    const DockerContainerAssessmentScanResult result{
        .containers = {make_assessment(id, analysis::CgroupMemoryRiskLevel::Low,
                                       analysis::CgroupMemoryAssessmentCoverage::Complete)},
        .skipped_container_count = 0,
        .warnings = {},
    };

    const std::string output = format_docker_container_assessment_scan_result(result);

    EXPECT_NE(output.find("Docker container assessments"), std::string::npos);
    EXPECT_NE(output.find("COVERAGE"), std::string::npos);
    EXPECT_NE(output.find(id), std::string::npos);
    EXPECT_NE(output.find("Low"), std::string::npos);
    EXPECT_NE(output.find("Complete"), std::string::npos);
    EXPECT_NE(output.find("None"), std::string::npos);
}

TEST(AssessmentsFormatter, FormatsDetailedFindingAndUnavailableEvidence) {
    const std::string id(64, 'b');
    const DockerContainerAssessment assessment = make_assessment(
        id, analysis::CgroupMemoryRiskLevel::Critical,
        analysis::CgroupMemoryAssessmentCoverage::Partial,
        {{.code = analysis::CgroupMemoryFindingCode::oom_kill_recorded,
          .risk = analysis::CgroupMemoryRiskLevel::Critical,
          .evidence = {{.code = analysis::CgroupMemoryEvidenceCode::memory_events_oom_kill,
                        .value = 1}},
          .recommendation = "Treat this as a workload incident."}},
        {{.code = CgroupMetricReadErrorCode::file_open_failed,
          .source_name = "memory.events",
          .detail = "Unable to open memory.events."}});

    const std::string output = format_docker_container_assessment_detail(assessment);

    EXPECT_NE(output.find("Overall risk"), std::string::npos);
    EXPECT_NE(output.find("Critical"), std::string::npos);
    EXPECT_NE(output.find("Coverage"), std::string::npos);
    EXPECT_NE(output.find("Partial"), std::string::npos);
    EXPECT_NE(output.find("1. oom_kill_recorded"), std::string::npos);
    EXPECT_NE(output.find("memory.events.oom_kill"), std::string::npos);
    EXPECT_NE(output.find("Unavailable evidence"), std::string::npos);
    EXPECT_NE(output.find("memory.events: Unable to open memory.events."), std::string::npos);
}

} // namespace
} // namespace memoptima::cli
