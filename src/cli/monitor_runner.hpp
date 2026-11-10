#pragma once

#include <chrono>
#include <expected>
#include <functional>
#include <iosfwd>
#include <string>

namespace memoptima::cli {

using FormattedStatusResult = std::expected<std::string, std::string>;

// Inject external effects so the monitor loop can be tested without real time or /proc reads.
struct MonitorDependencies {
    std::function<std::chrono::system_clock::time_point()> current_utc_time;
    std::function<std::chrono::steady_clock::time_point()> current_steady_time;
    std::function<FormattedStatusResult()> read_formatted_status;
    std::function<void(std::chrono::milliseconds)> sleep_for;
    std::function<bool()> stop_requested;
};

[[nodiscard]] std::string format_utc_timestamp(std::chrono::system_clock::time_point time_point);

void run_monitor(const MonitorDependencies &dependencies, std::ostream &standard_output,
                 std::ostream &standard_error);

} // namespace memoptima::cli
