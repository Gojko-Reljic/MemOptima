#include "cli/formatting.hpp"

#include <array>
#include <format>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr std::uint64_t kBytesPerUnit = 1024;
constexpr std::array<std::string_view, 5> kByteUnitNames{"B", "KiB", "MiB", "GiB", "TiB"};

} // namespace

std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0)
        return "0 B";

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};
    while (display_value >= static_cast<double>(kBytesPerUnit) &&
           unit_index + 1 < kByteUnitNames.size()) {
        display_value /= static_cast<double>(kBytesPerUnit);
        ++unit_index;
    }
    return std::format("{:.2f} {}", display_value, kByteUnitNames[unit_index]);
}

std::string format_percentage(double value, std::size_t decimal_places) {
    return std::format("{:.{}f}%", value, decimal_places);
}

std::string format_duration(std::uint64_t microseconds) {
    if (microseconds < 1000)
        return std::format("{} µs", microseconds);

    const double milliseconds = static_cast<double>(microseconds) / 1000.0;
    return milliseconds < 1000.0 ? std::format("{:.2f} ms", milliseconds)
                                 : std::format("{:.2f} s", milliseconds / 1000.0);
}

std::string format_operational_duration(std::chrono::milliseconds duration) {
    using namespace std::chrono;

    if (duration < seconds{1})
        return "< 1s";

    const auto total_seconds = static_cast<std::uint64_t>(duration_cast<seconds>(duration).count());
    constexpr std::uint64_t kSecondsPerMinute = 60;
    constexpr std::uint64_t kSecondsPerHour = 60 * kSecondsPerMinute;
    constexpr std::uint64_t kSecondsPerDay = 24 * kSecondsPerHour;

    if (total_seconds < kSecondsPerMinute)
        return std::format("{}s", total_seconds);

    const auto minutes = total_seconds / kSecondsPerMinute;
    const auto seconds = total_seconds % kSecondsPerMinute;
    if (total_seconds < kSecondsPerHour)
        return std::format("{}m {:02d}s", minutes, seconds);

    const auto hours = total_seconds / kSecondsPerHour;
    const auto remaining_minutes = (total_seconds % kSecondsPerHour) / kSecondsPerMinute;
    if (total_seconds < kSecondsPerDay)
        return std::format("{}h {:02d}m", hours, remaining_minutes);

    const auto days = total_seconds / kSecondsPerDay;
    const auto remaining_hours = (total_seconds % kSecondsPerDay) / kSecondsPerHour;
    return std::format("{}d {:02d}h", days, remaining_hours);
}

std::string escape_terminal_cell(std::string_view value) {
    constexpr std::string_view hexadecimal_digits{"0123456789ABCDEF"};

    std::string escaped;
    escaped.reserve(value.size());
    for (const auto character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default: {
            const auto byte = static_cast<unsigned char>(character);
            if (byte <= 0x1FU || byte == 0x7FU) {
                escaped += "\\x";
                escaped += hexadecimal_digits[byte >> 4U];
                escaped += hexadecimal_digits[byte & 0x0FU];
            } else {
                escaped += character;
            }
            break;
        }
        }
    }
    return escaped;
}

} // namespace memoptima::cli
