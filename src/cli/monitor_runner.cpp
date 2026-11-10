#include "cli/monitor_runner.hpp"

#include <algorithm>
#include <array>
#include <ctime>
#include <ostream>

namespace memoptima::cli {
namespace {

constexpr auto kMeasurementInterval = std::chrono::seconds{5};
constexpr auto kStopCheckInterval = std::chrono::milliseconds{100};

void wait_until_next_measurement(const MonitorDependencies& dependencies) {
    const auto deadline = dependencies.current_steady_time() + kMeasurementInterval;

    while (!dependencies.stop_requested()) {
        const auto remaining = deadline - dependencies.current_steady_time();
        if (remaining <= std::chrono::steady_clock::duration::zero()) {
            return;
        }

        const auto remaining_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(remaining);
        if (remaining_milliseconds == std::chrono::milliseconds::zero()) {
            return;
        }

        dependencies.sleep_for(std::min(kStopCheckInterval, remaining_milliseconds));
    }
}

}  // namespace

std::string format_utc_timestamp(std::chrono::system_clock::time_point time_point) {
    const std::time_t calendar_time = std::chrono::system_clock::to_time_t(time_point);
    std::tm utc_time{};

    if (gmtime_r(&calendar_time, &utc_time) == nullptr) {
        return "unavailable-utc-time";
    }

    std::array<char, 21> buffer{};
    if (std::strftime(buffer.data(), buffer.size(), "%Y-%m-%dT%H:%M:%SZ", &utc_time) == 0) {
        return "unavailable-utc-time";
    }

    return buffer.data();
}

void run_monitor(
    const MonitorDependencies& dependencies,
    std::ostream& standard_output,
    std::ostream& standard_error) {
    while (!dependencies.stop_requested()) {
        const std::string timestamp = format_utc_timestamp(dependencies.current_utc_time());
        const FormattedStatusResult status = dependencies.read_formatted_status();

        if (status.has_value()) {
            standard_output << timestamp << "\n\n" << *status << '\n';
        } else {
            standard_error << timestamp << " Error: " << status.error() << '\n';
        }

        wait_until_next_measurement(dependencies);
    }
}

}  // namespace memoptima::cli
