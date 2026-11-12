#include <memoptima/utc_timestamp.hpp>

#include <array>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace memoptima {

std::expected<std::string, UtcTimestampFormatError>
format_utc_timestamp(std::chrono::system_clock::time_point value, UtcTimestampPrecision precision) {
    const auto whole_seconds = std::chrono::floor<std::chrono::seconds>(value);
    const std::time_t calendar_time = std::chrono::system_clock::to_time_t(whole_seconds);
    std::tm utc_time{};
    if (gmtime_r(&calendar_time, &utc_time) == nullptr) {
        return std::unexpected(UtcTimestampFormatError{
            .code = UtcTimestampFormatErrorCode::utc_conversion_failed,
            .detail = "Unable to convert the system-clock value to UTC.",
        });
    }

    std::array<char, 21> second_precision_buffer{};
    if (std::strftime(second_precision_buffer.data(), second_precision_buffer.size(),
                      "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0) {
        return std::unexpected(UtcTimestampFormatError{
            .code = UtcTimestampFormatErrorCode::utc_conversion_failed,
            .detail = "Unable to format the UTC calendar value.",
        });
    }

    if (precision == UtcTimestampPrecision::seconds) {
        return second_precision_buffer.data();
    }

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(value - whole_seconds).count();
    std::ostringstream output;
    output << std::string_view{second_precision_buffer.data(), 19} << '.' << std::setw(3)
           << std::setfill('0') << milliseconds << 'Z';
    return output.str();
}

} // namespace memoptima
