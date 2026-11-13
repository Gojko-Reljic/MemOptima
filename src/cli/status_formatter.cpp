#include "cli/status_formatter.hpp"
#include "cli/formatting.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

namespace memoptima::cli {
namespace {

[[nodiscard]] std::string_view pressure_level_name(analysis::MemoryPressureLevel level) {
    switch (level) {
    case analysis::MemoryPressureLevel::Normal:
        return "Normal";
    case analysis::MemoryPressureLevel::Warning:
        return "Warning";
    case analysis::MemoryPressureLevel::Critical:
        return "Critical";
    }

    return "Critical";
}

[[nodiscard]] std::string_view recommendation_for(analysis::MemoryPressureLevel level) {
    switch (level) {
    case analysis::MemoryPressureLevel::Normal:
        return "No action is required.";
    case analysis::MemoryPressureLevel::Warning:
        return "Review memory-intensive processes.";
    case analysis::MemoryPressureLevel::Critical:
        return "Close unnecessary applications and review memory-intensive processes.";
    }

    return "Close unnecessary applications and review memory-intensive processes.";
}

void append_pressure_reasons(std::ostringstream &output, const analysis::MemoryPressure &pressure) {
    if (pressure.has_reason(analysis::MemoryPressureReason::AvailableMemoryAtCriticalThreshold)) {
        output << "  Reason:             Available RAM is at or below the critical threshold.\n";
    } else if (pressure.has_reason(
                   analysis::MemoryPressureReason::AvailableMemoryAtWarningThreshold)) {
        output << "  Reason:             Available RAM is at or below the warning threshold.\n";
    }

    if (pressure.has_reason(analysis::MemoryPressureReason::HighSwapUsage)) {
        output << "  Reason:             Swap usage is at or above 50%.\n";
    }
}

} // namespace

std::string format_memory_status(const analysis::MemoryStatus &status,
                                 const analysis::MemoryPressure &pressure) {
    std::ostringstream output;
    output << "Memory status\n\n"
           << "RAM\n"
           << "  Total:              " << format_bytes(status.total_memory_bytes) << '\n'
           << "  Available:          " << format_bytes(status.available_memory_bytes) << '\n'
           << "  In use (estimate): " << format_bytes(status.used_memory_bytes) << " ("
           << format_percentage(status.used_memory_percentage, 1) << ")\n\n"
           << "Swap\n"
           << "  Total:              " << format_bytes(status.total_swap_bytes) << '\n'
           << "  Free:               " << format_bytes(status.free_swap_bytes) << '\n'
           << "  In use:             " << format_bytes(status.used_swap_bytes) << " ("
           << (status.used_swap_percentage ? format_percentage(*status.used_swap_percentage, 1)
                                           : "N/A")
           << ")\n\n"
           << "Memory pressure\n"
           << "  Level:              " << pressure_level_name(pressure.level()) << '\n';
    append_pressure_reasons(output, pressure);
    output << "  Recommendation:     " << recommendation_for(pressure.level()) << '\n';
    return output.str();
}

} // namespace memoptima::cli
