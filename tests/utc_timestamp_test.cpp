#include <memoptima/utc_timestamp.hpp>

#include <gtest/gtest.h>

#include <chrono>

namespace memoptima {
namespace {

TEST(UtcTimestamp, FormatsSecondsInCanonicalIso8601Utc) {
    const auto timestamp = format_utc_timestamp(
        std::chrono::system_clock::time_point{std::chrono::seconds{1'700'000'000}},
        UtcTimestampPrecision::seconds);

    ASSERT_TRUE(timestamp.has_value());
    EXPECT_EQ(*timestamp, "2023-11-14T22:13:20Z");
}

TEST(UtcTimestamp, FormatsMillisecondsInCanonicalIso8601Utc) {
    const auto timestamp = format_utc_timestamp(
        std::chrono::system_clock::time_point{std::chrono::milliseconds{1'700'000'000'123}},
        UtcTimestampPrecision::milliseconds);

    ASSERT_TRUE(timestamp.has_value());
    EXPECT_EQ(*timestamp, "2023-11-14T22:13:20.123Z");
}

TEST(UtcTimestamp, FloorsTimesBeforeTheUnixEpochForMilliseconds) {
    const auto timestamp =
        format_utc_timestamp(std::chrono::system_clock::time_point{std::chrono::milliseconds{-1}},
                             UtcTimestampPrecision::milliseconds);

    ASSERT_TRUE(timestamp.has_value());
    EXPECT_EQ(*timestamp, "1969-12-31T23:59:59.999Z");
}

} // namespace
} // namespace memoptima
