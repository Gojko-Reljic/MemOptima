#include <memoptima/analysis/memory_status.hpp>

#include <gtest/gtest.h>

namespace memoptima::analysis {
namespace {

TEST(MemoryStatus, CalculatesMemoryAndSwapUsage) {
    const auto snapshot = MemorySnapshot::create(1000, 250, 400, 100);

    ASSERT_TRUE(snapshot.has_value());
    const MemoryStatus status = evaluate_memory_status(*snapshot);

    EXPECT_EQ(status.used_memory_bytes, 750U);
    EXPECT_DOUBLE_EQ(status.used_memory_percentage, 75.0);
    EXPECT_EQ(status.used_swap_bytes, 300U);
    ASSERT_TRUE(status.used_swap_percentage.has_value());
    EXPECT_DOUBLE_EQ(*status.used_swap_percentage, 75.0);
}

TEST(MemoryStatus, ReportsNoSwapPercentageWhenSwapIsNotConfigured) {
    const auto snapshot = MemorySnapshot::create(1000, 250, 0, 0);

    ASSERT_TRUE(snapshot.has_value());
    const MemoryStatus status = evaluate_memory_status(*snapshot);

    EXPECT_EQ(status.used_swap_bytes, 0U);
    EXPECT_FALSE(status.used_swap_percentage.has_value());
}

}  // namespace
}  // namespace memoptima::analysis
