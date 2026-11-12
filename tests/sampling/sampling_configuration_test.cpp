#include <memoptima/sampling/sampling_configuration.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string_view>

namespace memoptima::sampling {
namespace {

TEST(SamplingCount, AcceptsConfiguredInclusiveBoundaries) {
    const auto minimum = SamplingCount::create(1);
    const auto maximum = SamplingCount::create(3600);

    ASSERT_TRUE(minimum.has_value());
    ASSERT_TRUE(maximum.has_value());
    EXPECT_EQ(minimum->value(), 1U);
    EXPECT_EQ(maximum->value(), 3600U);
}

TEST(SamplingCount, RejectsValuesOutsideTheConfiguredRange) {
    const auto zero = SamplingCount::create(0);
    const auto too_large = SamplingCount::create(3601);

    ASSERT_FALSE(zero.has_value());
    ASSERT_FALSE(too_large.has_value());
    EXPECT_EQ(zero.error().code, SamplingConfigurationErrorCode::count_out_of_range);
    EXPECT_EQ(too_large.error().code, SamplingConfigurationErrorCode::count_out_of_range);
}

TEST(SamplingCount, ParsesCanonicalDecimalValues) {
    const auto parsed = parse_sampling_count("12");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->value(), 12U);
}

TEST(SamplingCount, RejectsNonCanonicalAndOverflowingText) {
    for (const std::string_view text :
         {"", "0", "0012", "+12", "12.0", " 12", "12 ", "-12", "18446744073709551616"}) {
        const auto parsed = parse_sampling_count(text);

        ASSERT_FALSE(parsed.has_value()) << text;
        EXPECT_EQ(parsed.error().code, text == "0" || text == "18446744073709551616"
                                           ? SamplingConfigurationErrorCode::count_out_of_range
                                           : SamplingConfigurationErrorCode::count_invalid_format)
            << text;
    }
}

TEST(SamplingInterval, AcceptsConfiguredInclusiveBoundaries) {
    const auto minimum = SamplingInterval::create(std::chrono::milliseconds{100});
    const auto maximum = SamplingInterval::create(std::chrono::minutes{60});

    ASSERT_TRUE(minimum.has_value());
    ASSERT_TRUE(maximum.has_value());
    EXPECT_EQ(minimum->value(), std::chrono::milliseconds{100});
    EXPECT_EQ(maximum->value(), std::chrono::hours{1});
}

TEST(SamplingInterval, RejectsValuesOutsideTheConfiguredRange) {
    const auto too_short = SamplingInterval::create(std::chrono::milliseconds{99});
    const auto too_long = SamplingInterval::create(std::chrono::minutes{61});

    ASSERT_FALSE(too_short.has_value());
    ASSERT_FALSE(too_long.has_value());
    EXPECT_EQ(too_short.error().code, SamplingConfigurationErrorCode::interval_out_of_range);
    EXPECT_EQ(too_long.error().code, SamplingConfigurationErrorCode::interval_out_of_range);
}

TEST(SamplingInterval, ParsesEverySupportedUnit) {
    const auto milliseconds = parse_sampling_interval("500ms");
    const auto seconds = parse_sampling_interval("5s");
    const auto minutes = parse_sampling_interval("2m");

    ASSERT_TRUE(milliseconds.has_value());
    ASSERT_TRUE(seconds.has_value());
    ASSERT_TRUE(minutes.has_value());
    EXPECT_EQ(milliseconds->value(), std::chrono::milliseconds{500});
    EXPECT_EQ(seconds->value(), std::chrono::seconds{5});
    EXPECT_EQ(minutes->value(), std::chrono::minutes{2});
}

TEST(SamplingInterval, RejectsInvalidFormatUnsupportedUnitsAndUnsafeValues) {
    for (const std::string_view text : {"", "5", "005s", "5 s", "5.0s", "+5s", "-5s"}) {
        const auto parsed = parse_sampling_interval(text);

        ASSERT_FALSE(parsed.has_value()) << text;
        EXPECT_EQ(parsed.error().code,
                  text == "5" ? SamplingConfigurationErrorCode::interval_unsupported_unit
                              : SamplingConfigurationErrorCode::interval_invalid_format)
            << text;
    }

    for (const std::string_view text : {"5S", "5h", "5us"}) {
        const auto parsed = parse_sampling_interval(text);

        ASSERT_FALSE(parsed.has_value()) << text;
        EXPECT_EQ(parsed.error().code, SamplingConfigurationErrorCode::interval_unsupported_unit)
            << text;
    }

    for (const std::string_view text : {"0ms", "99ms", "61m", "18446744073709551616ms"}) {
        const auto parsed = parse_sampling_interval(text);

        ASSERT_FALSE(parsed.has_value()) << text;
        EXPECT_EQ(parsed.error().code, SamplingConfigurationErrorCode::interval_out_of_range)
            << text;
    }
}

} // namespace
} // namespace memoptima::sampling
