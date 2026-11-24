#include "cli/forecast_evaluation_formatter.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace memoptima::cli {
namespace {

using forecast_evaluation::ForecastEvaluationReport;
using forecast_evaluation::ForecastEvaluationRow;
using forecast_evaluation::ForecastEvaluationStatus;

std::vector<std::string> lines(std::string_view value) {
    std::vector<std::string> result;
    std::size_t position{};
    while (position < value.size()) {
        const auto end = value.find('\n', position);
        if (end == std::string_view::npos)
            break;
        result.emplace_back(value.substr(position, end - position));
        position = end + 1U;
    }
    return result;
}

ForecastEvaluationReport report_with_row(ForecastEvaluationStatus status) {
    const bool evaluated = status == ForecastEvaluationStatus::evaluated;
    ForecastEvaluationReport report{};
    report.summary = {.row_count = 1U,
                      .eligible_forecast_count = evaluated ? 1U : 0U,
                      .evaluated_forecast_count = evaluated ? 1U : 0U,
                      .excluded_incomplete_session_count = 0U,
                      .status_counts = {{status, 1U}},
                      .coverage = evaluated ? std::optional<double>{1.0} : std::nullopt,
                      .mean_absolute_error_bytes = 1'048'576.0,
                      .mean_signed_error_bytes = -0.0001};
    report.rows.push_back({
        .identity = {.container_id = "aaaaaaaaaaaa111111111111",
                     .cgroup_path = "/sys/fs/cgroup/docker/example"},
        .epoch_index = 1U,
        .history_first_timestamp = forecast_evaluation::ForecastEvaluationTimePoint{},
        .history_last_timestamp =
            forecast_evaluation::ForecastEvaluationTimePoint{std::chrono::seconds{10}},
        .holdout_timestamp =
            forecast_evaluation::ForecastEvaluationTimePoint{std::chrono::seconds{15}},
        .history_observation_count = 3U,
        .history_window_ms = 10'000U,
        .status = status,
        .predicted_current_bytes = evaluated ? std::optional<double>{131'072.0} : std::nullopt,
        .actual_current_bytes = evaluated ? std::optional<std::uint64_t>{130'048U} : std::nullopt,
        .signed_error_bytes = evaluated ? std::optional<double>{1'024.0} : std::nullopt,
        .absolute_error_bytes = evaluated ? std::optional<double>{1'024.0} : std::nullopt,
    });
    return report;
}

TEST(ForecastEvaluationFormatter, PrintsExactEmptyEvaluationMessage) {
    EXPECT_EQ(format_forecast_evaluation({}), "No forecast evaluation epochs found in manifest.\n");
}

TEST(ForecastEvaluationFormatter, FormatsSummaryTableAndCanonicalTimestamps) {
    const auto output =
        format_forecast_evaluation(report_with_row(ForecastEvaluationStatus::evaluated));

    EXPECT_NE(output.find("Eligible forecasts:   1"), std::string::npos);
    EXPECT_NE(output.find("Evaluated forecasts:  1"), std::string::npos);
    EXPECT_NE(output.find("Coverage:             100.00%"), std::string::npos);
    EXPECT_NE(output.find("MAE:                  1.00 MiB"), std::string::npos);
    EXPECT_NE(output.find("Bias:                 +0.00 B"), std::string::npos);
    EXPECT_NE(output.find("CONTAINER ID"), std::string::npos);
    EXPECT_NE(output.find("FORECAST AT"), std::string::npos);
    EXPECT_NE(output.find("HOLDOUT AT"), std::string::npos);
    EXPECT_NE(output.find("1970-01-01T00:00:10.000Z"), std::string::npos);
    EXPECT_NE(output.find("1970-01-01T00:00:15.000Z"), std::string::npos);
    EXPECT_NE(output.find("128.00 KiB"), std::string::npos);
    EXPECT_NE(output.find("+1.00 KiB"), std::string::npos);
    EXPECT_NE(output.find("evaluated"), std::string::npos);

    for (const auto &line : lines(output))
        EXPECT_FALSE(line.ends_with(' '));
}

TEST(ForecastEvaluationFormatter, MarksUnavailableValuesAndStatuses) {
    const auto output = format_forecast_evaluation(
        report_with_row(ForecastEvaluationStatus::holdout_observation_unavailable));

    EXPECT_NE(output.find("N/A"), std::string::npos);
    EXPECT_NE(output.find("holdout observation unavailable"), std::string::npos);
}

TEST(ForecastEvaluationFormatter, UsesSharedUnambiguousPrefixesForRepeatedContainerIdentifiers) {
    auto report = report_with_row(ForecastEvaluationStatus::evaluated);
    auto first_identifier = std::string(64U, 'a');
    first_identifier[12] = 'b';
    auto second_identifier = std::string(64U, 'a');
    second_identifier[12] = 'c';

    report.rows[0].identity.container_id = first_identifier;
    auto repeated_row = report.rows[0];
    repeated_row.epoch_index = 2U;
    auto second_row = report.rows[0];
    second_row.identity.container_id = second_identifier;
    second_row.epoch_index = 1U;
    report.rows.push_back(std::move(repeated_row));
    report.rows.push_back(std::move(second_row));

    const auto output = format_forecast_evaluation(report);

    EXPECT_NE(output.find(first_identifier.substr(0U, 13U)), std::string::npos);
    EXPECT_NE(output.find(second_identifier.substr(0U, 13U)), std::string::npos);
}

} // namespace
} // namespace memoptima::cli
