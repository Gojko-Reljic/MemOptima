#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <string>

namespace memoptima::forecast_evaluation {
namespace {
using namespace std::chrono_literals;

[[nodiscard]] Observation observation(std::uint64_t bytes) {
    return {
        .identity = {.container_id = std::string(64U, 'a'), .cgroup_path = "/docker/evaluation"},
        .current_memory_bytes = bytes};
}

void sample(ForecastEvaluationAnalyzer &analyzer, ForecastEvaluationTimePoint time,
            std::uint64_t bytes) {
    ASSERT_TRUE(analyzer.begin_sample({.captured_at = time}).has_value());
    ASSERT_TRUE(analyzer.observe(observation(bytes)).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
}

TEST(ForecastEvaluationAnalyzerTest, EvaluatesRollingOriginsAtTheConfiguredHorizon) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 10'000U}).has_value());
    const auto start = ForecastEvaluationTimePoint{};
    for (std::uint64_t index{}; index < 12U; ++index)
        sample(analyzer, start + 10s * index, 100U + 10U * index);
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows[0].horizon, 1min);
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::evaluated);
    EXPECT_EQ(report->rows[0].origin_count, 4U);
    EXPECT_DOUBLE_EQ(*report->rows[0].mean_absolute_error_bytes, 0.0);
}

TEST(ForecastEvaluationAnalyzerTest, ReportsHorizonBelowSamplingIntervalExplicitly) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 120'000U}).has_value());
    const auto start = ForecastEvaluationTimePoint{};
    for (std::uint64_t index{}; index < 4U; ++index)
        sample(analyzer, start + 2min * index, 100U + index);
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::horizon_below_sampling_interval);
}

TEST(ForecastEvaluationAnalyzerTest, KeepsAnEpochLevelRowWhenHistoryIsNotUsable) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 1'000U}).has_value());
    sample(analyzer, ForecastEvaluationTimePoint{}, 100U);
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_FALSE(report->rows[0].horizon.has_value());
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::insufficient_observations);
}

TEST(ForecastEvaluationAnalyzerTest, ReportsUnavailableHoldoutForEligibleHistory) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 10'000U}).has_value());
    const auto start = ForecastEvaluationTimePoint{};
    for (std::uint64_t index{}; index < 3U; ++index)
        sample(analyzer, start + 5s * index, 100U + index);
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::holdout_observation_unavailable);
}

TEST(ForecastEvaluationAnalyzerTest, DeterministicallyBoundsRetainedOrigins) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 1'000U}).has_value());
    const auto start = ForecastEvaluationTimePoint{};
    for (std::uint64_t index{}; index < 400U; ++index) {
        sample(analyzer, start + 1s * index, 1'000U + index);
    }
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::evaluated);
    EXPECT_LE(report->rows[0].origin_count, 256U);
    EXPECT_EQ(report->rows[0].origin_count, 165U);
}

TEST(ForecastEvaluationAnalyzerTest, AggregatesRelativeErrorAndSkillPerHorizon) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 10'000U}).has_value());
    const auto start = ForecastEvaluationTimePoint{};
    for (std::uint64_t index{}; index < 12U; ++index) {
        sample(analyzer, start + 10s * index, 1'048'576U + 10U * index);
    }
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->summary.horizons.size(), 1U);
    const auto &summary = report->summary.horizons.front();
    EXPECT_EQ(summary.contributing_epoch_count, 1U);
    EXPECT_DOUBLE_EQ(*summary.mean_epoch_absolute_error_bytes, 0.0);
    EXPECT_DOUBLE_EQ(*summary.mean_epoch_relative_absolute_error, 0.0);
    ASSERT_TRUE(summary.skill.has_value());
    EXPECT_GT(*summary.skill, 0.999);
}

TEST(ForecastEvaluationAnalyzerTest, NeverCarriesOriginsAcrossSessionBoundaries) {
    ForecastEvaluationAnalyzer analyzer{{{.duration = 1min}}};
    const auto start = ForecastEvaluationTimePoint{};
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 10'000U}).has_value());
    for (std::uint64_t index{}; index < 10U; ++index) {
        sample(analyzer, start + 10s * index, 100U + 10U * index);
    }
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    ASSERT_TRUE(analyzer.begin_session({.requested_interval_ms = 10'000U}).has_value());
    for (std::uint64_t index{}; index < 10U; ++index) {
        sample(analyzer, start + 2h + 10s * index, 1'000U + 10U * index);
    }
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 2U);
    EXPECT_EQ(report->rows[0].epoch_index, 1U);
    EXPECT_EQ(report->rows[1].epoch_index, 2U);
}
} // namespace
} // namespace memoptima::forecast_evaluation
