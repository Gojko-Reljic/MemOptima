#include "cli/monitor_runner.hpp"

#include <memoptima/utc_timestamp.hpp>

#include <algorithm>
#include <ostream>

namespace memoptima::cli {
namespace {

// Measure every five seconds while polling often enough to respond promptly to SIGINT.
constexpr auto kMeasurementInterval = std::chrono::seconds{5};
constexpr auto kStopCheckInterval = std::chrono::milliseconds{100};

void wait_until_next_measurement(const MonitorDependencies &dependencies) {
    // A monotonic clock keeps wall-clock adjustments from changing the measurement interval.
    const auto deadline = dependencies.current_steady_time() + kMeasurementInterval;

    while (!dependencies.stop_requested()) {
        const auto remaining = deadline - dependencies.current_steady_time();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return;
        }

        const auto remaining_milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
        if (remaining_milliseconds == std::chrono::milliseconds::zero()) {
            return;
        }

        dependencies.sleep_for(std::min(kStopCheckInterval, remaining_milliseconds));
    }
}

} // namespace

void run_monitor(const MonitorDependencies &dependencies, std::ostream &standard_output,
                 std::ostream &standard_error) {
    while (!dependencies.stop_requested()) {
        const auto formatted_timestamp =
            format_utc_timestamp(dependencies.current_utc_time(), UtcTimestampPrecision::seconds);
        const std::string timestamp =
            formatted_timestamp.has_value() ? *formatted_timestamp : "unavailable-utc-time";
        const FormattedStatusResult status = dependencies.read_formatted_status();

        if (status.has_value()) {
            // Keep measurements pipeable while reserving standard error for timestamped failures.
            standard_output << timestamp << "\n\n" << *status << '\n';
        } else {
            standard_error << timestamp << " Error: " << status.error() << '\n';
        }

        wait_until_next_measurement(dependencies);
    }
}

} // namespace memoptima::cli
