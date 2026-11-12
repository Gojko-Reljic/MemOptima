#include "cli/monitor_runner.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>
#include <vector>

namespace memoptima::cli {
namespace {

class MonitorSimulation {
  public:
    explicit MonitorSimulation(std::vector<FormattedStatusResult> results)
        : results_{std::move(results)} {}

    MonitorDependencies dependencies() {
        return {
            .current_utc_time =
                [this] {
                    return utc_start_ +
                           std::chrono::duration_cast<std::chrono::system_clock::duration>(
                               elapsed_);
                },
            .current_steady_time = [this] { return steady_start_ + elapsed_; },
            .read_formatted_status = [this] { return results_.at(read_count_++); },
            .sleep_for = [this](std::chrono::milliseconds duration) { elapsed_ += duration; },
            .stop_requested = [this] { return read_count_ >= results_.size(); },
        };
    }

  private:
    const std::chrono::system_clock::time_point utc_start_{std::chrono::seconds{1'700'000'000}};
    const std::chrono::steady_clock::time_point steady_start_{};
    std::vector<FormattedStatusResult> results_;
    std::chrono::milliseconds elapsed_{};
    std::size_t read_count_{};
};

TEST(MonitorRunner, WritesEachSuccessfulMeasurementToStandardOutput) {
    MonitorSimulation simulation(
        {FormattedStatusResult{"First status\n"}, FormattedStatusResult{"Second status\n"}});
    std::ostringstream standard_output;
    std::ostringstream standard_error;

    run_monitor(simulation.dependencies(), standard_output, standard_error);

    EXPECT_EQ(standard_output.str(), R"(2023-11-14T22:13:20Z

First status

2023-11-14T22:13:25Z

Second status

)");
    EXPECT_TRUE(standard_error.str().empty());
}

TEST(MonitorRunner, ReportsErrorsToStandardErrorAndContinuesMonitoring) {
    MonitorSimulation simulation({
        std::unexpected(std::string{"unable to read system memory information: temporary failure"}),
        FormattedStatusResult{"Recovered status\n"},
    });
    std::ostringstream standard_output;
    std::ostringstream standard_error;

    run_monitor(simulation.dependencies(), standard_output, standard_error);

    EXPECT_EQ(standard_error.str(), "2023-11-14T22:13:20Z Error: unable to read system memory "
                                    "information: temporary failure\n");
    EXPECT_EQ(standard_output.str(), R"(2023-11-14T22:13:25Z

Recovered status

)");
}

} // namespace
} // namespace memoptima::cli
