#include <memoptima/trends/trend_analyzer.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>

namespace memoptima::trends {
namespace {

constexpr TrendSessionStart session_start{.started_at = TrendTimePoint{},
                                          .requested_interval_ms = 10'000};
constexpr TrendSampleStart sample_start{.captured_at = session_start.started_at};

TrendObservation observation() {
    return {.identity = {.container_id = std::string(64, 'a'), .cgroup_path = "/docker/first"},
            .current_memory_bytes = 10,
            .maximum_memory = {.kind = TrendMemoryMaximumKind::finite, .bytes = 100}};
}

template <typename Value>
void expect_error(const std::expected<Value, TrendAnalyzerError> &result,
                  TrendAnalyzerErrorCode code) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, code);
    EXPECT_FALSE(result.error().detail.empty());
}

TEST(TrendAnalyzerTest, AcceptsSamplesAcrossCompleteAndIncompleteSessions) {
    TrendAnalyzer analyzer;

    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    ASSERT_TRUE(analyzer.observe(observation()).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(
        analyzer.begin_sample({sample_start.captured_at + std::chrono::seconds{10}}).has_value());
    ASSERT_TRUE(analyzer.observe(observation()).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto next_start = session_start.started_at + std::chrono::seconds{20};
    ASSERT_TRUE(
        analyzer.begin_session({next_start, session_start.requested_interval_ms}).has_value());
    ASSERT_TRUE(analyzer.begin_sample({next_start}).has_value());
    ASSERT_TRUE(analyzer.observe(observation()).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(analyzer.end_session({.incomplete = true}).has_value());
    EXPECT_TRUE(analyzer.finish().has_value());
}

TEST(TrendAnalyzerTest, RejectsMisorderedEventsAndKeepsTheActiveState) {
    TrendAnalyzer analyzer;
    constexpr auto invalid_sequence = TrendAnalyzerErrorCode::invalid_event_sequence;

    expect_error(analyzer.begin_sample(sample_start), invalid_sequence);
    expect_error(analyzer.observe(observation()), invalid_sequence);
    expect_error(analyzer.end_sample(), invalid_sequence);
    expect_error(analyzer.end_session({.incomplete = false}), invalid_sequence);

    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    expect_error(analyzer.begin_session(session_start), invalid_sequence);
    expect_error(analyzer.observe(observation()), invalid_sequence);
    expect_error(analyzer.end_sample(), invalid_sequence);
    expect_error(analyzer.finish(), invalid_sequence);

    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    expect_error(analyzer.begin_session(session_start), invalid_sequence);
    expect_error(analyzer.begin_sample(sample_start), invalid_sequence);
    expect_error(analyzer.end_session({.incomplete = false}), invalid_sequence);
    expect_error(analyzer.finish(), invalid_sequence);

    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    EXPECT_TRUE(analyzer.finish().has_value());
}

TEST(TrendAnalyzerTest, ReturnsAnEmptyAnalysisForNoSessions) {
    TrendAnalyzer analyzer;

    const auto analysis = analyzer.finish();

    ASSERT_TRUE(analysis.has_value());
    EXPECT_TRUE(analysis->epochs.empty());
    EXPECT_EQ(analysis->summary.epoch_count, 0U);
    EXPECT_EQ(analysis->summary.continuous_epoch_count, 0U);
    EXPECT_EQ(analysis->summary.sparse_epoch_count, 0U);
    EXPECT_EQ(analysis->summary.incomplete_epoch_count, 0U);
    EXPECT_EQ(analysis->summary.rate_available_epoch_count, 0U);
    EXPECT_EQ(analysis->summary.time_to_limit_available_epoch_count, 0U);
}

TEST(TrendAnalyzerTest, RejectsEveryEventAfterFinishing) {
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.finish().has_value());
    constexpr auto invalid_sequence = TrendAnalyzerErrorCode::invalid_event_sequence;

    expect_error(analyzer.begin_session(session_start), invalid_sequence);
    expect_error(analyzer.begin_sample(sample_start), invalid_sequence);
    expect_error(analyzer.observe(observation()), invalid_sequence);
    expect_error(analyzer.end_sample(), invalid_sequence);
    expect_error(analyzer.end_session({.incomplete = false}), invalid_sequence);
    expect_error(analyzer.finish(), invalid_sequence);
}

TEST(TrendAnalyzerTest, DetectsDuplicateObservationsByBothPartsOfTheIdentity) {
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    auto value = observation();

    ASSERT_TRUE(analyzer.observe(value).has_value());
    value.current_memory_bytes = std::nullopt;
    expect_error(analyzer.observe(value), TrendAnalyzerErrorCode::duplicate_observation);

    value.identity.cgroup_path = "/docker/second";
    ASSERT_TRUE(analyzer.observe(value).has_value());
    value.identity = observation().identity;
    value.identity.container_id = std::string(64, 'b');
    EXPECT_TRUE(analyzer.observe(value).has_value());
}

TEST(TrendAnalyzerTest, BoundsDistinctIdentitiesPerSampleAndPreservesDuplicateErrors) {
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    auto value = observation();
    value.current_memory_bytes = std::nullopt;

    constexpr std::size_t sample_identity_limit = 4096U;
    for (std::size_t index = 0; index < sample_identity_limit; ++index) {
        value.identity.cgroup_path = "/docker/container-" + std::to_string(index);
        ASSERT_TRUE(analyzer.observe(value).has_value()) << "Identity index: " << index;
    }
    expect_error(analyzer.observe(value), TrendAnalyzerErrorCode::duplicate_observation);

    value.identity.cgroup_path = "/docker/overflow";
    expect_error(analyzer.observe(value), TrendAnalyzerErrorCode::resource_limit_exceeded);

    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(
        analyzer.begin_sample({sample_start.captured_at + std::chrono::seconds{10}}).has_value());
    EXPECT_TRUE(analyzer.observe(value).has_value());
}

TEST(TrendAnalyzerTest, ValidatesMaximumMemoryWithoutReservingAnInvalidObservationIdentity) {
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    auto value = observation();
    constexpr auto invalid_observation = TrendAnalyzerErrorCode::invalid_observation;

    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unlimited, .bytes = 1};
    expect_error(analyzer.observe(value), invalid_observation);
    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unavailable, .bytes = 1};
    expect_error(analyzer.observe(value), invalid_observation);

    value.maximum_memory = {.kind = TrendMemoryMaximumKind::finite, .bytes = 0};
    EXPECT_TRUE(analyzer.observe(value).has_value());
    value.identity.cgroup_path = "/docker/unlimited";
    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unlimited};
    EXPECT_TRUE(analyzer.observe(value).has_value());
    value.identity.cgroup_path = "/docker/unavailable";
    value.current_memory_bytes = std::nullopt;
    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unavailable};
    EXPECT_TRUE(analyzer.observe(value).has_value());
}

} // namespace
} // namespace memoptima::trends
