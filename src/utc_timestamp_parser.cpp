#include "utc_timestamp_parser.hpp"

#include <charconv>
#include <cstdint>
#include <iomanip>
#include <locale>
#include <sstream>

namespace memoptima::detail {
namespace {

[[nodiscard]] bool is_ascii_decimal_digit(char character) noexcept {
    return character >= '0' && character <= '9';
}

[[nodiscard]] std::expected<unsigned, UtcTimestampParseError>
parse_component(std::string_view text) {
    unsigned value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::unexpected(UtcTimestampParseError{
            .detail = "The UTC timestamp contains an invalid numeric component.",
        });
    }
    return value;
}

} // namespace

std::expected<std::chrono::sys_time<std::chrono::milliseconds>, UtcTimestampParseError>
parse_canonical_utc_timestamp(std::string_view text) {
    constexpr std::size_t kTimestampLength = 24;
    if (text.size() != kTimestampLength || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != '.' || text[23] != 'Z') {
        return std::unexpected(UtcTimestampParseError{
            .detail = "Expected an ISO 8601 UTC timestamp with milliseconds.",
        });
    }

    for (const std::size_t index :
         {0U, 1U, 2U, 3U, 5U, 6U, 8U, 9U, 11U, 12U, 14U, 15U, 17U, 18U, 20U, 21U, 22U}) {
        if (!is_ascii_decimal_digit(text[index])) {
            return std::unexpected(UtcTimestampParseError{
                .detail = "Expected an ISO 8601 UTC timestamp with milliseconds.",
            });
        }
    }

    const auto year = parse_component(text.substr(0, 4));
    const auto month = parse_component(text.substr(5, 2));
    const auto day = parse_component(text.substr(8, 2));
    const auto hour = parse_component(text.substr(11, 2));
    const auto minute = parse_component(text.substr(14, 2));
    const auto second = parse_component(text.substr(17, 2));
    const auto millisecond = parse_component(text.substr(20, 3));
    if (!year.has_value() || !month.has_value() || !day.has_value() || !hour.has_value() ||
        !minute.has_value() || !second.has_value() || !millisecond.has_value()) {
        return std::unexpected(UtcTimestampParseError{
            .detail = "The UTC timestamp contains an invalid numeric component.",
        });
    }

    const std::chrono::year_month_day date{std::chrono::year{static_cast<int>(*year)},
                                           std::chrono::month{*month}, std::chrono::day{*day}};
    if (!date.ok() || *hour > 23 || *minute > 59 || *second > 59) {
        return std::unexpected(UtcTimestampParseError{
            .detail = "The UTC timestamp contains an invalid calendar value.",
        });
    }

    const std::chrono::sys_days midnight{date};
    return std::chrono::sys_time<std::chrono::milliseconds>{midnight} + std::chrono::hours{*hour} +
           std::chrono::minutes{*minute} + std::chrono::seconds{*second} +
           std::chrono::milliseconds{*millisecond};
}

std::string format_canonical_utc_timestamp(std::chrono::sys_time<std::chrono::milliseconds> value) {
    const auto day = std::chrono::floor<std::chrono::days>(value);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time_of_day{value - day};
    std::ostringstream output;
    // Evidence timestamps are a schema contract, so their digits must not inherit user locale
    // rules.
    output.imbue(std::locale::classic());
    output << std::setfill('0') << std::setw(4) << static_cast<int>(date.year()) << '-'
           << std::setw(2) << static_cast<unsigned>(date.month()) << '-' << std::setw(2)
           << static_cast<unsigned>(date.day()) << 'T' << std::setw(2)
           << time_of_day.hours().count() << ':' << std::setw(2) << time_of_day.minutes().count()
           << ':' << std::setw(2) << time_of_day.seconds().count() << '.' << std::setw(3)
           << time_of_day.subseconds().count() << 'Z';
    return output.str();
}

} // namespace memoptima::detail
