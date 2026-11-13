#include "cli/processes_formatter.hpp"
#include "cli/formatting.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace memoptima::cli {
namespace {

// Fixed minimum widths keep the terminal table readable; long process names remain intact.
constexpr int kPidColumnWidth = 8;
constexpr int kNameColumnWidth = 21;

} // namespace

std::string format_process_scan_result(const linux::ProcessScanResult &result) {
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
           << std::left << std::setw(kPidColumnWidth) << "PID" << std::setw(kNameColumnWidth)
           << "NAME"
           << "RESIDENT MEMORY\n";

    for (const ProcessMemoryInfo &process : result.processes) {
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

} // namespace memoptima::cli
