#include "cli/containers_formatter.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::cli {
namespace {

[[nodiscard]] DockerContainerMemorySnapshot make_container(std::string id, std::uint64_t current,
                                                           std::optional<std::uint64_t> high,
                                                           std::optional<std::uint64_t> maximum,
                                                           std::uint64_t oom_kills,
                                                           double psi_some_average_10) {
    const auto parsed_id = DockerContainerId::parse(id);
    EXPECT_TRUE(parsed_id.has_value());
    return DockerContainerMemorySnapshot::create(
        *parsed_id, "/sys/fs/cgroup/docker/" + id,
        CgroupMemorySnapshot::create(
            CgroupByteMetric{current},
            {.high = CgroupMemoryLimitMetric{high}, .maximum = CgroupMemoryLimitMetric{maximum}},
            {.high = CgroupEventMetric{0},
             .maximum = CgroupEventMetric{0},
             .oom = CgroupEventMetric{0},
             .oom_kill = CgroupEventMetric{oom_kills}},
            {.anonymous = CgroupByteMetric{0}, .file_backed = CgroupByteMetric{0}},
            CgroupByteMetric{0},
            {.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{.average_10_seconds =
                                                                             psi_some_average_10,
                                                                         .average_60_seconds = 0,
                                                                         .average_300_seconds = 0,
                                                                         .total_microseconds = 0}},
             .full =
                 CgroupMetric<CgroupPressureLine>{CgroupPressureLine{.average_10_seconds = 0,
                                                                     .average_60_seconds = 0,
                                                                     .average_300_seconds = 0,
                                                                     .total_microseconds = 0}}}));
}

TEST(ContainersFormatter, FormatsTheContainerMemoryTable) {
    const std::string id(64, 'a');
    const DockerContainerScanResult result = DockerContainerScanResult::create(
        {make_container(id, 1024ULL * 1024ULL, std::nullopt, 2ULL * 1024ULL * 1024ULL, 3, 2.375)},
        0, {});

    const std::string output = format_docker_container_scan_result(result);

    EXPECT_NE(output.find("Docker container memory"), std::string::npos);
    EXPECT_NE(output.find("CONTAINER ID"), std::string::npos);
    EXPECT_NE(output.find(id), std::string::npos);
    EXPECT_NE(output.find("1.00 MiB"), std::string::npos);
    EXPECT_NE(output.find("max"), std::string::npos);
    EXPECT_NE(output.find("2.00 MiB"), std::string::npos);
    EXPECT_NE(output.find("3"), std::string::npos);
    EXPECT_NE(output.find("2.38%"), std::string::npos);
}

TEST(ContainersFormatter, FormatsNoContainerMessage) {
    const DockerContainerScanResult result = DockerContainerScanResult::create({}, 0, {});

    EXPECT_EQ(format_docker_container_scan_result(result),
              "No supported rootful Docker containers found.\n");
}

TEST(ContainersFormatter, FormatsWarningsForStandardError) {
    const DockerContainerScanResult result = DockerContainerScanResult::create(
        {}, 0,
        {{.code = DockerContainerScanWarningCode::docker_cgroup_layout_not_found,
          .cgroup_path = "/sys/fs/cgroup",
          .detail = "No standard rootful Docker cgroup v2 layout was found."}});

    const std::string output = format_docker_container_scan_warnings(result);

    EXPECT_NE(output.find("Warning:"), std::string::npos);
    EXPECT_NE(output.find("No standard rootful Docker cgroup v2 layout was found."),
              std::string::npos);
    EXPECT_NE(output.find("/sys/fs/cgroup"), std::string::npos);
}

TEST(ContainersFormatter, FormatsACompleteContainerDetailReport) {
    const std::string id(64, 'b');
    const DockerContainerMemorySnapshot container =
        make_container(id, 1024ULL * 1024ULL, std::nullopt, 2ULL * 1024ULL * 1024ULL, 3, 2.375);

    const std::string output = format_docker_container_detail(container);

    EXPECT_NE(output.find("Container"), std::string::npos);
    EXPECT_NE(output.find("Memory\n"), std::string::npos);
    EXPECT_NE(output.find("Events"), std::string::npos);
    EXPECT_NE(output.find("Composition"), std::string::npos);
    EXPECT_NE(output.find("Swap"), std::string::npos);
    EXPECT_NE(output.find("Memory pressure"), std::string::npos);
    EXPECT_NE(output.find("total=0 µs"), std::string::npos);
}

TEST(ContainersFormatter, DeduplicatesUnavailableDataCauses) {
    const std::string id(64, 'c');
    const auto parsed_id = DockerContainerId::parse(id);
    ASSERT_TRUE(parsed_id.has_value());
    const CgroupMetricReadError error{.code = CgroupMetricReadErrorCode::file_open_failed,
                                      .source_name = "memory.events",
                                      .detail = "Unable to open memory.events."};
    const DockerContainerMemorySnapshot container = DockerContainerMemorySnapshot::create(
        *parsed_id, "/sys/fs/cgroup/docker/" + id,
        CgroupMemorySnapshot::create(
            CgroupByteMetric{0},
            {.high = CgroupMemoryLimitMetric{std::nullopt},
             .maximum = CgroupMemoryLimitMetric{std::nullopt}},
            {.high = std::unexpected(error),
             .maximum = std::unexpected(error),
             .oom = std::unexpected(error),
             .oom_kill = std::unexpected(error)},
            {.anonymous = CgroupByteMetric{0}, .file_backed = CgroupByteMetric{0}},
            CgroupByteMetric{0},
            {.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}},
             .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}}}));

    const std::string output = format_docker_container_detail(container);

    EXPECT_NE(output.find("Unavailable data"), std::string::npos);
    EXPECT_EQ(output.find("memory.events: Unable to open memory.events."),
              output.rfind("memory.events: Unable to open memory.events."));
}

} // namespace
} // namespace memoptima::cli
