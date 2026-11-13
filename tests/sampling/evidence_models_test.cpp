#include <memoptima/sampling/evidence_models.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::sampling {
namespace {

[[nodiscard]] DockerContainerMemorySnapshot make_snapshot(std::string id) {
    const auto parsed_id = DockerContainerId::parse(id);
    EXPECT_TRUE(parsed_id.has_value());
    return {
        .container_id = *parsed_id,
        .cgroup_path = "/sys/fs/cgroup/docker/" + id,
        .memory =
            {
                .current_memory = CgroupByteMetric{512},
                .limits = CgroupMemoryLimits{.high = CgroupMemoryLimitMetric{1024},
                                             .maximum = CgroupMemoryLimitMetric{2048}},
                .events = CgroupMemoryEvents{.high = CgroupEventMetric{0},
                                             .maximum = CgroupEventMetric{0},
                                             .oom = CgroupEventMetric{0},
                                             .oom_kill = CgroupEventMetric{0}},
                .composition = CgroupMemoryComposition{.anonymous = CgroupByteMetric{256},
                                                       .file_backed = CgroupByteMetric{128}},
                .current_swap = CgroupByteMetric{0},
                .pressure =
                    CgroupMemoryPressure{
                        .some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{
                            .average_10_seconds = 0.0,
                            .average_60_seconds = 0.0,
                            .average_300_seconds = 0.0,
                            .total_microseconds = 0,
                        }},
                        .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{
                            .average_10_seconds = 0.0,
                            .average_60_seconds = 0.0,
                            .average_300_seconds = 0.0,
                            .total_microseconds = 0,
                        }}},
            },
    };
}

TEST(DockerContainerEvidence, PreservesRawSnapshotAndDerivesAssessment) {
    const std::string id(64, 'a');
    const DockerContainerEvidence evidence = DockerContainerEvidence::create(make_snapshot(id));

    EXPECT_EQ(evidence.snapshot().container_id.value(), id);
    EXPECT_EQ(*evidence.snapshot().memory.current_memory, 512U);
    EXPECT_EQ(evidence.assessment().overall_risk(), analysis::CgroupMemoryRiskLevel::Low);
    EXPECT_TRUE(evidence.assessment().findings().empty());
}

TEST(SamplingEvidenceSample, PreservesTimingContainersAndDiscoveryDiagnostics) {
    const auto sequence = SamplingCount::create(3);
    ASSERT_TRUE(sequence.has_value());
    const auto captured_at =
        std::chrono::system_clock::time_point{std::chrono::milliseconds{123456}};
    const std::string id(64, 'b');
    const DockerContainerScanWarning warning{
        .code = DockerContainerScanWarningCode::invalid_container_identifier,
        .cgroup_path = "/sys/fs/cgroup/docker/not-an-id",
        .detail = "Ignoring a directory without a valid Docker container ID.",
    };

    const SamplingEvidenceSample sample = SamplingEvidenceSample::create(
        *sequence, std::chrono::seconds{10}, captured_at, std::chrono::milliseconds{12},
        {DockerContainerEvidence::create(make_snapshot(id))}, 1, {warning});

    EXPECT_EQ(sample.sequence().value(), 3U);
    EXPECT_EQ(sample.planned_offset(), std::chrono::seconds{10});
    EXPECT_EQ(sample.captured_at(), captured_at);
    EXPECT_EQ(sample.schedule_delay(), std::chrono::milliseconds{12});
    ASSERT_EQ(sample.containers().size(), 1U);
    EXPECT_EQ(sample.containers().front().snapshot().container_id.value(), id);
    EXPECT_EQ(sample.skipped_container_count(), 1U);
    ASSERT_EQ(sample.discovery_warnings().size(), 1U);
    EXPECT_EQ(sample.discovery_warnings().front().code,
              DockerContainerScanWarningCode::invalid_container_identifier);
}

} // namespace
} // namespace memoptima::sampling
