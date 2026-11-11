#include <memoptima/cgroup_memory_snapshot.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(CgroupMemorySnapshot, PreservesAvailableMetricsAlongsideUnavailableMetrics) {
    const CgroupMetricReadError unavailable{
        .code = CgroupMetricReadErrorCode::file_open_failed,
        .source_name = "memory.swap.current",
        .detail = "Unable to open memory.swap.current.",
    };

    const CgroupMemorySnapshot snapshot = CgroupMemorySnapshot::create(
        CgroupByteMetric{1024},
        CgroupMemoryLimits{
            .high = CgroupMemoryLimitMetric{std::optional<std::uint64_t>{}},
            .maximum = CgroupMemoryLimitMetric{2048},
        },
        CgroupMemoryEvents{.high = CgroupByteMetric{1},
                           .maximum = CgroupByteMetric{2},
                           .oom = CgroupByteMetric{3},
                           .oom_kill = CgroupByteMetric{4}},
        CgroupMemoryComposition{.anonymous = CgroupByteMetric{900},
                                .file_backed = CgroupByteMetric{124}},
        std::unexpected(unavailable),
        CgroupMemoryPressure{.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}},
                             .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}}});

    ASSERT_TRUE(snapshot.current_memory().has_value());
    EXPECT_EQ(*snapshot.current_memory(), 1024U);
    ASSERT_TRUE(snapshot.limits().high.has_value());
    EXPECT_FALSE(snapshot.limits().high->has_value());
    ASSERT_FALSE(snapshot.current_swap().has_value());
    EXPECT_EQ(snapshot.current_swap().error().code, CgroupMetricReadErrorCode::file_open_failed);
}

TEST(CgroupMemorySnapshot, RetainsCurrentMemoryAboveTheConfiguredMaximum) {
    const CgroupMemorySnapshot snapshot = CgroupMemorySnapshot::create(
        CgroupByteMetric{4096},
        CgroupMemoryLimits{.high = CgroupMemoryLimitMetric{3072},
                           .maximum = CgroupMemoryLimitMetric{2048}},
        CgroupMemoryEvents{.high = CgroupByteMetric{0},
                           .maximum = CgroupByteMetric{1},
                           .oom = CgroupByteMetric{0},
                           .oom_kill = CgroupByteMetric{0}},
        CgroupMemoryComposition{.anonymous = CgroupByteMetric{4096},
                                .file_backed = CgroupByteMetric{0}},
        CgroupByteMetric{0},
        CgroupMemoryPressure{.some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}},
                             .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{}}});

    ASSERT_TRUE(snapshot.current_memory().has_value());
    ASSERT_TRUE(snapshot.limits().maximum.has_value());
    ASSERT_TRUE(snapshot.limits().maximum->has_value());
    EXPECT_GT(*snapshot.current_memory(), **snapshot.limits().maximum);
}

} // namespace
} // namespace memoptima
