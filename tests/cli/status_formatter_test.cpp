#include "cli/status_formatter.hpp"

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

    EXPECT_EQ(format_memory_status(status), R"(Memory status

RAM
  Total:              1.00 GiB
  Available:          512.00 MiB
  In use (estimate): 512.00 MiB (50.0%)

Swap
  Total:              2.00 GiB
  Free:               512.00 MiB
  In use:             1.50 GiB (75.0%)
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

    EXPECT_NE(format_memory_status(status).find("In use:             0 B (N/A)"), std::string::npos);
}

}  // namespace
}  // namespace memoptima::cli
