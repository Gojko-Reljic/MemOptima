#include "cli/status_formatter.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr std::uint64_t kBytesPerUnit = 1024;
constexpr std::array<std::string_view, 5> kUnitNames{"B", "KiB", "MiB", "GiB", "TiB"};

[[nodiscard]] std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0) {
        return "0 B";
    }

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};

    while (display_value >= static_cast<double>(kBytesPerUnit) && unit_index + 1 < kUnitNames.size()) {
        display_value /= static_cast<double>(kBytesPerUnit);
        ++unit_index;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << display_value << ' ' << kUnitNames[unit_index];
    return output.str();
}

[[nodiscard]] std::string format_percentage(const std::optional<double>& percentage) {
    if (!percentage.has_value()) {
        return "N/A";
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(1) << *percentage << '%';
    return output.str();
}

}  // namespace

std::string format_memory_status(const analysis::MemoryStatus& status) {
    std::ostringstream output;
    output << "Memory status\n\n"
           << "RAM\n"
           << "  Total:              " << format_bytes(status.total_memory_bytes) << '\n'
           << "  Available:          " << format_bytes(status.available_memory_bytes) << '\n'
           << "  In use (estimate): " << format_bytes(status.used_memory_bytes) << " ("
           << format_percentage(status.used_memory_percentage) << ")\n\n"
           << "Swap\n"
           << "  Total:              " << format_bytes(status.total_swap_bytes) << '\n'
           << "  Free:               " << format_bytes(status.free_swap_bytes) << '\n'
           << "  In use:             " << format_bytes(status.used_swap_bytes) << " ("
           << format_percentage(status.used_swap_percentage) << ")\n";
    return output.str();
}

}  // namespace memoptima::cli
