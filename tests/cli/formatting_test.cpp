#include "cli/formatting.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace memoptima::cli {
namespace {

TEST(Formatting, FormatsBytesAtUnitBoundaries) {
    EXPECT_EQ(format_bytes(0), "0 B");
    EXPECT_EQ(format_bytes(1023), "1023.00 B");
    EXPECT_EQ(format_bytes(1024), "1.00 KiB");
    EXPECT_EQ(format_bytes(1024ULL * 1024ULL), "1.00 MiB");
}

TEST(Formatting, FormatsPercentagesWithRequestedPrecision) {
    EXPECT_EQ(format_percentage(12.34, 1), "12.3%");
    EXPECT_EQ(format_percentage(12.34, 2), "12.34%");
}

TEST(Formatting, FormatsPressureDurationsWithStableUnits) {
    EXPECT_EQ(format_duration(999), "999 µs");
    EXPECT_EQ(format_duration(1000), "1.00 ms");
    EXPECT_EQ(format_duration(1000000), "1.00 s");
}

} // namespace
} // namespace memoptima::cli
