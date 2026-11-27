#include "cli/forecast_evaluation_formatter.hpp"

#include <gtest/gtest.h>

namespace memoptima::cli {
TEST(ForecastEvaluationFormatter, PrintsExactEmptyEvaluationMessage) {
    EXPECT_EQ(format_forecast_evaluation({}), "No forecast evaluation epochs found in manifest.\n");
}

TEST(ForecastEvaluationFormatter, FormatsHorizonAndAggregatedMetrics) {
    using namespace std::chrono_literals;
    forecast_evaluation::ForecastEvaluationReport report{};
    report.summary.row_count = 1U;
    report.summary.evaluated_epoch_horizon_count = 1U;
    report.summary.status_counts.emplace(forecast_evaluation::ForecastEvaluationStatus::evaluated,
                                         1U);
    report.rows.push_back(
        {.identity = {.container_id = "container", .cgroup_path = "/docker/example"},
         .epoch_index = 1U,
         .horizon = 1min,
         .origin_count = 3U,
         .status = forecast_evaluation::ForecastEvaluationStatus::evaluated,
         .mean_absolute_error_bytes = 1024.0,
         .mean_persistence_absolute_error_bytes = 2048.0,
         .mean_signed_error_bytes = -0.0001,
         .relative_mean_absolute_error = 0.001});

    const auto output = format_forecast_evaluation(report);

    EXPECT_NE(output.find("HORIZON"), std::string::npos);
    EXPECT_NE(output.find("ORIGINS"), std::string::npos);
    EXPECT_NE(output.find("1m 00s"), std::string::npos);
    EXPECT_NE(output.find("1.00 KiB"), std::string::npos);
    EXPECT_NE(output.find("+0.00 B"), std::string::npos);
    EXPECT_NE(output.find("evaluated"), std::string::npos);
}
} // namespace memoptima::cli
