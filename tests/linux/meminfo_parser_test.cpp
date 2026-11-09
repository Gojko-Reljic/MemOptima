#include "linux/meminfo_parser.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace memoptima::linux::detail {
namespace {

constexpr std::string_view kValidMemoryInfo = R"(MemTotal:       32747752 kB
MemFree:        21252284 kB
MemAvailable:   27303452 kB
Buffers:            9328 kB
Cached:          6466868 kB
SwapTotal:       8388604 kB
SwapFree:        8388604 kB
)";

TEST(MemInfoParser, ParsesRequiredMetricsAndConvertsKilobytesToBytes) {
    const auto result = parse_memory_info(kValidMemoryInfo);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->total_memory_bytes, 32747752ULL * 1024ULL);
    EXPECT_EQ(result->available_memory_bytes, 27303452ULL * 1024ULL);
    EXPECT_EQ(result->total_swap_bytes, 8388604ULL * 1024ULL);
    EXPECT_EQ(result->free_swap_bytes, 8388604ULL * 1024ULL);
}

TEST(MemInfoParser, RejectsMissingRequiredMetric) {
    const auto result = parse_memory_info(R"(MemTotal: 100 kB
MemAvailable: 50 kB
SwapTotal: 25 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::missing_required_metric);
    EXPECT_EQ(result.error().metric_name, "SwapFree");
    EXPECT_EQ(result.error().line_number, 0U);
}

TEST(MemInfoParser, RejectsDuplicateRequiredMetric) {
    const auto result = parse_memory_info(R"(MemTotal: 100 kB
MemTotal: 100 kB
MemAvailable: 50 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::duplicate_required_metric);
    EXPECT_EQ(result.error().metric_name, "MemTotal");
    EXPECT_EQ(result.error().line_number, 2U);
}

TEST(MemInfoParser, RejectsInvalidNumericValue) {
    const auto result = parse_memory_info(R"(MemTotal: invalid kB
MemAvailable: 50 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::invalid_value);
    EXPECT_EQ(result.error().metric_name, "MemTotal");
    EXPECT_EQ(result.error().line_number, 1U);
}

TEST(MemInfoParser, RejectsUnexpectedUnit) {
    const auto result = parse_memory_info(R"(MemTotal: 100 MiB
MemAvailable: 50 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::unexpected_unit);
    EXPECT_EQ(result.error().metric_name, "MemTotal");
    EXPECT_EQ(result.error().line_number, 1U);
}

TEST(MemInfoParser, RejectsValuesThatExceedUnsigned64BitRange) {
    const auto result = parse_memory_info(R"(MemTotal: 18446744073709551616 kB
MemAvailable: 50 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::value_overflow);
    EXPECT_EQ(result.error().metric_name, "MemTotal");
    EXPECT_EQ(result.error().line_number, 1U);
}

TEST(MemInfoParser, RejectsValuesThatOverflowDuringByteConversion) {
    const auto result = parse_memory_info(R"(MemTotal: 18446744073709551615 kB
MemAvailable: 50 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::value_overflow);
    EXPECT_EQ(result.error().metric_name, "MemTotal");
    EXPECT_EQ(result.error().line_number, 1U);
}

TEST(MemInfoParser, RejectsInconsistentMetricValues) {
    const auto result = parse_memory_info(R"(MemTotal: 100 kB
MemAvailable: 101 kB
SwapTotal: 25 kB
SwapFree: 10 kB
)");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MemoryReadErrorCode::inconsistent_values);
    EXPECT_EQ(result.error().metric_name, "MemAvailable");
    EXPECT_EQ(result.error().line_number, 2U);
}

}  // namespace
}  // namespace memoptima::linux::detail
