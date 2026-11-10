#include "cli/processes_formatter.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr int kPidColumnWidth = 8;
constexpr int kNameColumnWidth = 21;

[[nodiscard]] std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0) {
        return "0 B";
    }

    constexpr std::uint64_t bytes_per_unit = 1024;
    constexpr std::array<std::string_view, 5> unit_names{"B", "KiB", "MiB", "GiB", "TiB"};

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};
    while (display_value >= static_cast<double>(bytes_per_unit) && unit_index + 1 < unit_names.size()) {
        display_value /= static_cast<double>(bytes_per_unit);
        ++unit_index;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << display_value << ' ' << unit_names[unit_index];
    return output.str();
}

}  // namespace

std::string format_process_scan_result(const linux::ProcessScanResult& result) {
    if (result.processes.empty()) {
        std::ostringstream output;
        output << "No readable process memory records were found.\n";
        if (result.skipped_process_count != 0) {
            output << "Note: " << result.skipped_process_count
                   << (result.skipped_process_count == 1
                           ? " process was skipped because its status could not be read.\n"
                           : " processes were skipped because their statuses could not be read.\n");
        }
        return output.str();
    }

    std::ostringstream output;
    output << "Top memory processes (VmRSS)\n\n"
           << std::left << std::setw(kPidColumnWidth) << "PID"
           << std::setw(kNameColumnWidth) << "NAME"
           << "RESIDENT MEMORY\n";

    for (const ProcessMemoryInfo& process : result.processes) {
        output << std::left << std::setw(kPidColumnWidth) << process.pid()
               << std::setw(kNameColumnWidth) << process.name()
               << format_bytes(process.resident_memory_bytes()) << '\n';
    }

    if (result.skipped_process_count != 0) {
        output << "\nNote: " << result.skipped_process_count
               << (result.skipped_process_count == 1
                       ? " process was skipped because its status could not be read.\n"
                       : " processes were skipped because their statuses could not be read.\n");
    }

    return output.str();
}

}  // namespace memoptima::cli
