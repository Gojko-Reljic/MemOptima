#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace memoptima::forecast_evaluation {
namespace {

using namespace std::chrono_literals;

[[nodiscard]] Observation observation(std::uint64_t bytes = 100U) {
    return {.identity = {.container_id = std::string(64, 'a'), .cgroup_path = "/docker/example"},
            .current_memory_bytes = bytes};
}

template <typename Value>
void expect_error(const std::expected<Value, ForecastEvaluationAnalyzerError> &result,
                  ForecastEvaluationAnalyzerErrorCode code) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, code);
    EXPECT_FALSE(result.error().detail.empty());
}

void accept_sample(ForecastEvaluationAnalyzer &analyzer, ForecastEvaluationTimePoint captured_at,
                   std::initializer_list<Observation> observations) {
    ASSERT_TRUE(analyzer.begin_sample({.captured_at = captured_at}).has_value());
    for (const auto &value : observations) {
        ASSERT_TRUE(analyzer.observe(value).has_value());
    }
    ASSERT_TRUE(analyzer.end_sample().has_value());
}

TEST(ForecastEvaluationAnalyzerTest, RejectsMisorderedEventsAndFinishesAnEmptyEvaluation) {
    ForecastEvaluationAnalyzer analyzer;
    constexpr auto invalid = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence;

    expect_error(analyzer.begin_sample({.captured_at = {}}), invalid);
    expect_error(analyzer.observe(observation()), invalid);
    expect_error(analyzer.end_sample(), invalid);
    expect_error(analyzer.end_session({.incomplete = false}), invalid);

    ASSERT_TRUE(analyzer.begin_session().has_value());
    expect_error(analyzer.begin_session(), invalid);
    expect_error(analyzer.finish(), invalid);
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report->rows.empty());
    EXPECT_EQ(report->summary.row_count, 0U);
    EXPECT_FALSE(report->summary.coverage.has_value());
    expect_error(analyzer.finish(), invalid);
}

TEST(ForecastEvaluationAnalyzerTest, RejectsDuplicateObservationsAndBoundsOneSample) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    ASSERT_TRUE(analyzer.begin_sample({.captured_at = {}}).has_value());
    auto value = observation();
    ASSERT_TRUE(analyzer.observe(value).has_value());
    expect_error(analyzer.observe(value),
                 ForecastEvaluationAnalyzerErrorCode::duplicate_observation);

    for (std::size_t index = 1U; index < 4096U; ++index) {
        value.identity.cgroup_path = "/docker/" + std::to_string(index);
        ASSERT_TRUE(analyzer.observe(value).has_value()) << index;
    }
    value.identity.cgroup_path = "/docker/overflow";
    expect_error(analyzer.observe(value),
                 ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded);
}

TEST(ForecastEvaluationAnalyzerTest, RejectsAnEmptyIdentityWithoutReservingIt) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    ASSERT_TRUE(analyzer.begin_sample({.captured_at = {}}).has_value());
    auto invalid = observation();
    invalid.identity.cgroup_path.clear();
    expect_error(analyzer.observe(invalid),
                 ForecastEvaluationAnalyzerErrorCode::invalid_observation);
    EXPECT_TRUE(analyzer.observe(observation()).has_value());
}

TEST(ForecastEvaluationAnalyzerTest,
     UsesTheFirstEligibleForecastAndTheImmediatelyFollowingHoldout) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(analyzer, start, {observation(100U)});
    accept_sample(analyzer, start + 5s, {observation(110U)});
    accept_sample(analyzer, start + 10s, {observation(120U)});
    accept_sample(analyzer, start + 15s, {observation(130U)});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    const auto &row = report->rows.front();
    EXPECT_EQ(row.status, ForecastEvaluationStatus::evaluated);
    EXPECT_EQ(row.history_observation_count, 3U);
    EXPECT_EQ(row.history_window_ms, 10'000U);
    EXPECT_EQ(row.actual_current_bytes, 130U);
    ASSERT_TRUE(row.predicted_current_bytes.has_value());
    EXPECT_DOUBLE_EQ(*row.predicted_current_bytes, 130.0);
    EXPECT_DOUBLE_EQ(*row.signed_error_bytes, 0.0);
    EXPECT_DOUBLE_EQ(*row.absolute_error_bytes, 0.0);
    EXPECT_EQ(report->summary.eligible_forecast_count, 1U);
    EXPECT_EQ(report->summary.evaluated_forecast_count, 1U);
    EXPECT_DOUBLE_EQ(*report->summary.coverage, 1.0);
}

TEST(ForecastEvaluationAnalyzerTest, ReportsAnEligibleForecastWithoutTheNextHoldout) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(analyzer, start, {observation(100U)});
    accept_sample(analyzer, start + 5s, {observation(110U)});
    accept_sample(analyzer, start + 10s, {observation(120U)});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows.front().status,
              ForecastEvaluationStatus::holdout_observation_unavailable);
    EXPECT_EQ(report->summary.eligible_forecast_count, 1U);
    EXPECT_EQ(report->summary.evaluated_forecast_count, 0U);
    EXPECT_DOUBLE_EQ(*report->summary.coverage, 0.0);
}

TEST(ForecastEvaluationAnalyzerTest, DistinguishesShortHistoryAndAnUnavailableHoldout) {
    ForecastEvaluationAnalyzer short_history;
    ASSERT_TRUE(short_history.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(short_history, start, {observation(100U)});
    accept_sample(short_history, start + 1s, {observation(110U)});
    accept_sample(short_history, start + 2s, {observation(120U)});
    ASSERT_TRUE(short_history.end_session({.incomplete = false}).has_value());
    const auto short_report = short_history.finish();
    ASSERT_TRUE(short_report.has_value());
    ASSERT_EQ(short_report->rows.size(), 1U);
    EXPECT_EQ(short_report->rows.front().status,
              ForecastEvaluationStatus::insufficient_observation_window);

    ForecastEvaluationAnalyzer unavailable_holdout;
    ASSERT_TRUE(unavailable_holdout.begin_session().has_value());
    accept_sample(unavailable_holdout, start, {observation(100U)});
    accept_sample(unavailable_holdout, start + 5s, {observation(110U)});
    accept_sample(unavailable_holdout, start + 10s, {observation(120U)});
    auto missing = observation();
    missing.current_memory_bytes = std::nullopt;
    accept_sample(unavailable_holdout, start + 15s, {missing});
    ASSERT_TRUE(unavailable_holdout.end_session({.incomplete = false}).has_value());
    const auto unavailable_report = unavailable_holdout.finish();
    ASSERT_TRUE(unavailable_report.has_value());
    ASSERT_EQ(unavailable_report->rows.size(), 1U);
    EXPECT_EQ(unavailable_report->rows.front().status,
              ForecastEvaluationStatus::holdout_observation_unavailable);
    EXPECT_EQ(unavailable_report->summary.eligible_forecast_count, 1U);
}

TEST(ForecastEvaluationAnalyzerTest, ClampsANegativeLinearProjectionToZero) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(analyzer, start, {observation(30U)});
    accept_sample(analyzer, start + 5s, {observation(20U)});
    accept_sample(analyzer, start + 10s, {observation(10U)});
    accept_sample(analyzer, start + 20s, {observation(1U)});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    ASSERT_TRUE(report->rows.front().predicted_current_bytes.has_value());
    EXPECT_DOUBLE_EQ(*report->rows.front().predicted_current_bytes, 0.0);
    EXPECT_DOUBLE_EQ(*report->rows.front().signed_error_bytes, -1.0);
}

TEST(ForecastEvaluationAnalyzerTest, ReportsContinuityAndTimestampBreaksExplicitly) {
    ForecastEvaluationAnalyzer absent;
    ASSERT_TRUE(absent.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(absent, start, {observation(100U)});
    accept_sample(absent, start + 5s, {});
    ASSERT_TRUE(absent.end_session({.incomplete = false}).has_value());
    const auto absent_report = absent.finish();
    ASSERT_TRUE(absent_report.has_value());
    ASSERT_EQ(absent_report->rows.size(), 1U);
    EXPECT_EQ(absent_report->rows.front().status,
              ForecastEvaluationStatus::container_not_observed_continuously);

    ForecastEvaluationAnalyzer unordered;
    ASSERT_TRUE(unordered.begin_session().has_value());
    accept_sample(unordered, start, {observation(100U)});
    accept_sample(unordered, start, {observation(110U)});
    ASSERT_TRUE(unordered.end_session({.incomplete = false}).has_value());
    const auto unordered_report = unordered.finish();
    ASSERT_TRUE(unordered_report.has_value());
    ASSERT_EQ(unordered_report->rows.size(), 2U);
    EXPECT_EQ(unordered_report->rows.front().status,
              ForecastEvaluationStatus::non_increasing_timestamps);
}

TEST(ForecastEvaluationAnalyzerTest, StartsANewEpochWhenTimeMovesBackwardAfterEvaluation) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(analyzer, start, {observation(100U)});
    accept_sample(analyzer, start + 5s, {observation(110U)});
    accept_sample(analyzer, start + 10s, {observation(120U)});
    accept_sample(analyzer, start + 20s, {observation(140U)});
    accept_sample(analyzer, start + 15s, {observation(130U)});
    accept_sample(analyzer, start + 20s, {observation(140U)});
    accept_sample(analyzer, start + 25s, {observation(150U)});
    accept_sample(analyzer, start + 30s, {observation(160U)});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 2U);
    EXPECT_EQ(report->rows[0].status, ForecastEvaluationStatus::evaluated);
    EXPECT_EQ(report->rows[0].epoch_index, 1U);
    EXPECT_EQ(report->rows[1].status, ForecastEvaluationStatus::evaluated);
    EXPECT_EQ(report->rows[1].epoch_index, 2U);
}

TEST(ForecastEvaluationAnalyzerTest, RepresentsUnavailableInitialCurrentMemory) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    auto missing = observation();
    missing.current_memory_bytes = std::nullopt;
    accept_sample(analyzer, ForecastEvaluationTimePoint{}, {missing});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows.front().status,
              ForecastEvaluationStatus::historical_current_memory_unavailable);
    EXPECT_EQ(report->rows.front().history_observation_count, 0U);
}

TEST(ForecastEvaluationAnalyzerTest, ExcludesAnIncompleteSessionFromAggregateMetrics) {
    ForecastEvaluationAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session().has_value());
    const auto start = ForecastEvaluationTimePoint{};
    accept_sample(analyzer, start, {observation(100U)});
    accept_sample(analyzer, start + 5s, {observation(110U)});
    accept_sample(analyzer, start + 10s, {observation(120U)});
    accept_sample(analyzer, start + 15s, {observation(130U)});
    ASSERT_TRUE(analyzer.end_session({.incomplete = true}).has_value());

    const auto report = analyzer.finish();
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows.front().status, ForecastEvaluationStatus::excluded_incomplete_session);
    EXPECT_EQ(report->summary.eligible_forecast_count, 0U);
    EXPECT_EQ(report->summary.evaluated_forecast_count, 0U);
    EXPECT_EQ(report->summary.excluded_incomplete_session_count, 1U);
}

} // namespace
} // namespace memoptima::forecast_evaluation
