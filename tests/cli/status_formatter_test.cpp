#include "cli/status_formatter.hpp"

#include <memoptima/analysis/memory_pressure.hpp>

#include <gtest/gtest.h>

namespace memoptima::cli {
namespace {

TEST(StatusFormatter, FormatsStatusWithIecUnitsAndPercentages) {
    const analysis::MemoryStatus status{
        .total_memory_bytes = 1024ULL * 1024ULL * 1024ULL,
        .available_memory_bytes = 512ULL * 1024ULL * 1024ULL,
        .used_memory_bytes = 512ULL * 1024ULL * 1024ULL,
        .used_memory_percentage = 50.0,
        .total_swap_bytes = 2ULL * 1024ULL * 1024ULL * 1024ULL,
        .free_swap_bytes = 512ULL * 1024ULL * 1024ULL,
        .used_swap_bytes = 1536ULL * 1024ULL * 1024ULL,
        .used_swap_percentage = 75.0,
    };

    const analysis::MemoryPressure pressure = analysis::evaluate_memory_pressure(status);

    EXPECT_EQ(format_memory_status(status, pressure), R"(Memory status

RAM
  Total:              1.00 GiB
  Available:          512.00 MiB
  In use (estimate): 512.00 MiB (50.0%)

Swap
  Total:              2.00 GiB
  Free:               512.00 MiB
  In use:             1.50 GiB (75.0%)

Memory pressure
  Level:              Warning
  Reason:             Swap usage is at or above 50%.
  Recommendation:     Review memory-intensive processes.
)");
}

TEST(StatusFormatter, FormatsNoSwapAsNotApplicable) {
    const analysis::MemoryStatus status{
        .total_memory_bytes = 1024ULL,
        .available_memory_bytes = 0,
        .used_memory_bytes = 1024ULL,
        .used_memory_percentage = 100.0,
        .total_swap_bytes = 0,
        .free_swap_bytes = 0,
        .used_swap_bytes = 0,
        .used_swap_percentage = std::nullopt,
    };

    const analysis::MemoryPressure pressure = analysis::evaluate_memory_pressure(status);
    const std::string output = format_memory_status(status, pressure);

    EXPECT_NE(output.find("In use:             0 B (N/A)"), std::string::npos);
    EXPECT_NE(output.find("Level:              Critical"), std::string::npos);
    EXPECT_NE(
        output.find("Reason:             Available RAM is at or below the critical threshold."),
        std::string::npos);
    EXPECT_NE(output.find("Recommendation:     Close unnecessary applications and review "
                          "memory-intensive processes."),
              std::string::npos);
}

TEST(StatusFormatter, FormatsBothReasonsInStableOrder) {
    const analysis::MemoryStatus status{
        .total_memory_bytes = 1000,
        .available_memory_bytes = 200,
        .used_memory_bytes = 800,
        .used_memory_percentage = 80.0,
        .total_swap_bytes = 400,
        .free_swap_bytes = 200,
        .used_swap_bytes = 200,
        .used_swap_percentage = 50.0,
    };

    const analysis::MemoryPressure pressure = analysis::evaluate_memory_pressure(status);
    const std::string output = format_memory_status(status, pressure);

    const std::size_t available_memory_reason =
        output.find("Reason:             Available RAM is at or below the warning threshold.");
    const std::size_t swap_usage_reason =
        output.find("Reason:             Swap usage is at or above 50%.");

    ASSERT_NE(available_memory_reason, std::string::npos);
    ASSERT_NE(swap_usage_reason, std::string::npos);
    EXPECT_LT(available_memory_reason, swap_usage_reason);
    EXPECT_NE(output.find("Level:              Critical"), std::string::npos);
}

TEST(StatusFormatter, FormatsNormalRecommendationWithoutReasons) {
    const analysis::MemoryStatus status{
        .total_memory_bytes = 1000,
        .available_memory_bytes = 201,
        .used_memory_bytes = 799,
        .used_memory_percentage = 79.9,
        .total_swap_bytes = 400,
        .free_swap_bytes = 201,
        .used_swap_bytes = 199,
        .used_swap_percentage = 49.75,
    };

    const analysis::MemoryPressure pressure = analysis::evaluate_memory_pressure(status);
    const std::string output = format_memory_status(status, pressure);

    EXPECT_NE(output.find("Level:              Normal"), std::string::npos);
    EXPECT_EQ(output.find("Reason:"), std::string::npos);
    EXPECT_NE(output.find("Recommendation:     No action is required."), std::string::npos);
}

} // namespace
} // namespace memoptima::cli
