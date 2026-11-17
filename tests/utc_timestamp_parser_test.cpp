#include "utc_timestamp_parser.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <locale>

namespace memoptima::detail {
namespace {

class GroupingNumberPunctuation final : public std::numpunct<char> {
  protected:
    [[nodiscard]] char do_thousands_sep() const override { return ','; }

    [[nodiscard]] std::string do_grouping() const override { return "\3"; }
};

TEST(UtcTimestampParser, ParsesEpochAndPreEpochValuesInUtc) {
    const auto epoch = parse_canonical_utc_timestamp("1970-01-01T00:00:00.000Z");
    const auto before_epoch = parse_canonical_utc_timestamp("1969-12-31T23:59:59.999Z");

    ASSERT_TRUE(epoch.has_value());
    ASSERT_TRUE(before_epoch.has_value());
    EXPECT_EQ(*epoch, std::chrono::sys_time<std::chrono::milliseconds>{});
    EXPECT_EQ(*before_epoch,
              std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::milliseconds{-1}});
}

TEST(UtcTimestampParser, ParsesLeapDayWithoutUsingLocalTime) {
    const auto timestamp = parse_canonical_utc_timestamp("2024-02-29T12:34:56.789Z");

    ASSERT_TRUE(timestamp.has_value());
    const std::chrono::sys_days day{std::chrono::year{2024} / 2 / 29};
    EXPECT_EQ(*timestamp, std::chrono::sys_time<std::chrono::milliseconds>{day} +
                              std::chrono::hours{12} + std::chrono::minutes{34} +
                              std::chrono::seconds{56} + std::chrono::milliseconds{789});
}

TEST(UtcTimestampParser, RejectsNonCanonicalAndInvalidCalendarValues) {
    for (const std::string_view text : {"2024-02-29T12:34:56Z", "2024-02-30T12:34:56.000Z",
                                        "2024-01-01T24:00:00.000Z", "2024-01-01T00:00:00.000+00"}) {
        EXPECT_FALSE(parse_canonical_utc_timestamp(text).has_value()) << text;
    }
}

TEST(UtcTimestampParser, FormatsCanonicalValuesIndependentlyOfTheGlobalLocale) {
    const auto timestamp = parse_canonical_utc_timestamp("2300-01-02T03:04:05.006Z");
    ASSERT_TRUE(timestamp.has_value());

    const std::locale original =
        std::locale::global(std::locale{std::locale::classic(), new GroupingNumberPunctuation});
    EXPECT_EQ(format_canonical_utc_timestamp(*timestamp), "2300-01-02T03:04:05.006Z");
    std::locale::global(original);
}

} // namespace
} // namespace memoptima::detail
