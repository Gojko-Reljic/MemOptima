#pragma once

#include <chrono>
#include <expected>
#include <string>

namespace memoptima {

enum class UtcTimestampPrecision {
    seconds,
    milliseconds,
};

enum class UtcTimestampFormatErrorCode {
    utc_conversion_failed,
};

struct UtcTimestampFormatError {
    UtcTimestampFormatErrorCode code;
    std::string detail;
};

[[nodiscard]] std::expected<std::string, UtcTimestampFormatError>
format_utc_timestamp(std::chrono::system_clock::time_point value, UtcTimestampPrecision precision);

} // namespace memoptima
