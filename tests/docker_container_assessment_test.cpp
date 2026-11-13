#include <memoptima/docker_container_assessment.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace memoptima {
namespace {

[[nodiscard]] DockerContainerMemorySnapshot make_container(std::string id, std::uint64_t current,
                                                           std::optional<std::uint64_t> maximum) {
    const auto parsed_id = DockerContainerId::parse(id);
    EXPECT_TRUE(parsed_id.has_value());
    return {
        .container_id = *parsed_id,
        .cgroup_path = "/sys/fs/cgroup/docker/" + id,
        .memory =
            {
                .current_memory = CgroupByteMetric{current},
                .limits = {.high = CgroupMemoryLimitMetric{std::nullopt},
                           .maximum = CgroupMemoryLimitMetric{maximum}},
                .events = {.high = CgroupEventMetric{0},
                           .maximum = CgroupEventMetric{0},
                           .oom = CgroupEventMetric{0},
                           .oom_kill = CgroupEventMetric{0}},
                .composition = {.anonymous = CgroupByteMetric{0},
                                .file_backed = CgroupByteMetric{0}},
                .current_swap = CgroupByteMetric{0},
                .pressure = {.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}},
                             .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}}},
            },
    };
}

TEST(DockerContainerAssessment, PreservesDockerIdentityAndGenericAssessment) {
    const std::string id(64, 'a');
    const DockerContainerAssessment assessment =
        assess_docker_container_memory_snapshot(make_container(id, 98, 100));

    EXPECT_EQ(assessment.container_id.value(), id);
    EXPECT_EQ(assessment.cgroup_path, "/sys/fs/cgroup/docker/" + id);
    EXPECT_EQ(assessment.assessment.overall_risk(), analysis::CgroupMemoryRiskLevel::High);
}

TEST(DockerContainerAssessment, PreservesScanOrderSkipCountAndWarnings) {
    const std::string first_id(64, 'a');
    const std::string second_id(64, 'b');
    const DockerContainerScanResult scan_result{
        .containers = {make_container(first_id, 0, std::nullopt),
                       make_container(second_id, 90, 100)},
        .skipped_container_count = 3,
        .warnings =
            {{.code = DockerContainerScanWarningCode::duplicate_container_identifier,
              .cgroup_path = "/sys/fs/cgroup/docker/duplicate",
              .detail =
                  "The Docker container ID is present in more than one standard cgroup location."}},
    };

    const DockerContainerAssessmentScanResult assessment_result =
        assess_docker_container_scan_result(scan_result);

    ASSERT_EQ(assessment_result.containers.size(), 2U);
    EXPECT_EQ(assessment_result.containers[0].container_id.value(), first_id);
    EXPECT_EQ(assessment_result.containers[1].container_id.value(), second_id);
    EXPECT_EQ(assessment_result.skipped_container_count, 3U);
    ASSERT_EQ(assessment_result.warnings.size(), 1U);
    EXPECT_EQ(assessment_result.warnings.front().code,
              DockerContainerScanWarningCode::duplicate_container_identifier);
}

} // namespace
} // namespace memoptima
