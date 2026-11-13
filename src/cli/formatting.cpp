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

} // namespace memoptima::cli
