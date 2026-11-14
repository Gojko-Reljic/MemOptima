#pragma once

#include <chrono>
#include <expected>
#include <string>
#include <string_view>

namespace memoptima::detail {

struct UtcTimestampParseError {
    std::string detail;
};

// Parses the schema-1 UTC millisecond representation without consulting local time-zone settings.
[[nodiscard]] std::expected<std::chrono::sys_time<std::chrono::milliseconds>,
                            UtcTimestampParseError>
parse_canonical_utc_timestamp(std::string_view text);

} // namespace memoptima::detail
