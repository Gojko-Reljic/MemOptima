#include <memoptima/analysis/memory_pressure.hpp>
#include <memoptima/analysis/memory_status.hpp>

#include <gtest/gtest.h>

namespace memoptima::analysis {
namespace {

MemoryStatus make_status(std::uint64_t total_memory_bytes, std::uint64_t available_memory_bytes,
                         std::uint64_t total_swap_bytes, std::uint64_t used_swap_bytes) {
    return {
        .total_memory_bytes = total_memory_bytes,
        .available_memory_bytes = available_memory_bytes,
        .used_memory_bytes = total_memory_bytes - available_memory_bytes,
        .used_memory_percentage = 0.0,
        .total_swap_bytes = total_swap_bytes,
        .free_swap_bytes = total_swap_bytes - used_swap_bytes,
        .used_swap_bytes = used_swap_bytes,
        .used_swap_percentage = std::nullopt,
    };
}

TEST(MemoryPressure, ReportsNormalWhenAvailableMemoryAndSwapUsageAreHealthy) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 201, 400, 199));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Normal);
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, ReportsWarningAtTheTwentyPercentAvailableMemoryBoundary) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 200, 400, 0));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Warning);
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, ReportsCriticalAtTheTenPercentAvailableMemoryBoundary) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 100, 400, 0));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Critical);
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, RaisesNormalToWarningAtTheFiftyPercentSwapBoundary) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 201, 400, 200));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Warning);
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, RaisesWarningToCriticalWhenSwapUsageIsHigh) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 200, 400, 200));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Critical);
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, DoesNotRaiseCriticalBeyondCriticalWhenSwapUsageIsHigh) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 100, 400, 200));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Critical);
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtWarningThreshold));
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::AvailableMemoryAtCriticalThreshold));
    EXPECT_TRUE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

TEST(MemoryPressure, IgnoresSwapWhenItIsNotConfigured) {
    const MemoryPressure pressure = evaluate_memory_pressure(make_status(1000, 201, 0, 0));

    EXPECT_EQ(pressure.level(), MemoryPressureLevel::Normal);
    EXPECT_FALSE(pressure.has_reason(MemoryPressureReason::HighSwapUsage));
}

} // namespace
} // namespace memoptima::analysis
