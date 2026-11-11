#include "linux/cgroup_v2/psi_parser.hpp"

#include <gtest/gtest.h>

#include <string_view>

namespace memoptima::linux::detail {
namespace {

constexpr std::string_view kValidMemoryPressure =
    R"(some avg10=1.25 avg60=2.50 avg300=3.75 total=123456
full avg10=0.25 avg60=0.50 avg300=0.75 total=45678
)";

TEST(PsiParser, ParsesCompleteMemoryPressureData) {
    const auto result = parse_memory_pressure(kValidMemoryPressure);

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result->some.average_10_seconds, 1.25);
    EXPECT_DOUBLE_EQ(result->some.average_60_seconds, 2.50);
    EXPECT_DOUBLE_EQ(result->some.average_300_seconds, 3.75);
    EXPECT_EQ(result->some.total_microseconds, 123456U);
    EXPECT_DOUBLE_EQ(result->full.average_10_seconds, 0.25);
    EXPECT_DOUBLE_EQ(result->full.average_60_seconds, 0.50);
    EXPECT_DOUBLE_EQ(result->full.average_300_seconds, 0.75);
    EXPECT_EQ(result->full.total_microseconds, 45678U);
}

TEST(PsiParser, RejectsMissingPressureType) {
    const auto result = parse_memory_pressure("some avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::missing_pressure_type);
}

TEST(PsiParser, RejectsDuplicatePressureType) {
    const auto result = parse_memory_pressure(
        "some avg10=1 avg60=2 avg300=3 total=4\nsome avg10=1 avg60=2 avg300=3 total=4\n"
        "full avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::duplicate_pressure_type);
}

TEST(PsiParser, RejectsMissingRequiredField) {
    const auto result = parse_memory_pressure(
        "some avg10=1 avg60=2 total=4\nfull avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::invalid_format);
}

TEST(PsiParser, RejectsDuplicateRequiredField) {
    const auto result = parse_memory_pressure(
        "some avg10=1 avg10=2 avg300=3 total=4\nfull avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::duplicate_required_field);
}

TEST(PsiParser, RejectsOutOfRangePercentage) {
    const auto result = parse_memory_pressure(
        "some avg10=101 avg60=2 avg300=3 total=4\nfull avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::value_out_of_range);
}

TEST(PsiParser, RejectsInvalidPercentage) {
    const auto result = parse_memory_pressure(
        "some avg10=NaN avg60=2 avg300=3 total=4\nfull avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::invalid_value);
}

TEST(PsiParser, RejectsTotalOverflow) {
    const auto result =
        parse_memory_pressure("some avg10=1 avg60=2 avg300=3 total=18446744073709551616\n"
                              "full avg10=1 avg60=2 avg300=3 total=4\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, PsiParseErrorCode::value_overflow);
}

} // namespace
} // namespace memoptima::linux::detail
