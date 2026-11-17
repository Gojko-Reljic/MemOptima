#include <memoptima/trends/trend_analyzer.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <initializer_list>
#include <limits>
#include <optional>
#include <string>

namespace memoptima::trends {
namespace {

using namespace std::chrono_literals;

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

void accept_sample(TrendAnalyzer &analyzer, TrendTimePoint captured_at,
                   std::initializer_list<TrendObservation> observations) {
    ASSERT_TRUE(analyzer.begin_sample({captured_at}).has_value());
    for (const auto &value : observations) {
        ASSERT_TRUE(analyzer.observe(value).has_value());
    }
    ASSERT_TRUE(analyzer.end_sample().has_value());
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
    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 1U);
    const auto &epoch = analysis->epochs.front();
    EXPECT_EQ(epoch.epoch_index, 1U);
    EXPECT_EQ(epoch.first_observed_at, sample_start.captured_at);
    EXPECT_EQ(epoch.last_observed_at, next_start);
    EXPECT_EQ(epoch.observation_count, 3U);
    EXPECT_EQ(epoch.observation_window_ms, 20'000U);
    EXPECT_EQ(epoch.largest_gap_ms, 10'000U);
    EXPECT_EQ(epoch.quality, TrendEvidenceQuality::continuous);
    EXPECT_TRUE(epoch.contains_incomplete_session);
    EXPECT_FALSE(epoch.termination_reason.has_value());
    EXPECT_EQ(epoch.current_memory.rate_status, TrendRateStatus::available);
    ASSERT_TRUE(epoch.current_memory.growth_bytes_per_second.has_value());
    EXPECT_DOUBLE_EQ(*epoch.current_memory.growth_bytes_per_second, 0.0);
    EXPECT_FALSE(epoch.time_to_limit.estimated_duration_ms.has_value());
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

TEST(TrendAnalyzerTest, ClosesEpochOnEverySemanticMaximumChange) {
    constexpr auto finite = TrendMemoryMaximumKind::finite;
    constexpr auto unlimited = TrendMemoryMaximumKind::unlimited;
    constexpr auto unavailable = TrendMemoryMaximumKind::unavailable;
    constexpr std::array maximums{
        TrendMemoryMaximum{finite, 100}, TrendMemoryMaximum{finite, 200},
        TrendMemoryMaximum{unlimited},   TrendMemoryMaximum{unavailable},
        TrendMemoryMaximum{unlimited},   TrendMemoryMaximum{finite, 0},
        TrendMemoryMaximum{unavailable}, TrendMemoryMaximum{finite, 100},
    };
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    auto value = observation();
    auto captured_at = session_start.started_at;
    std::uint64_t bytes = 10U;
    for (const auto maximum : maximums) {
        value.maximum_memory = maximum;
        value.current_memory_bytes = bytes++;
        accept_sample(analyzer, captured_at, {value});
        captured_at += 10s;
    }
    value.current_memory_bytes = bytes;
    accept_sample(analyzer, captured_at, {value});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), maximums.size());
    for (std::size_t index = 0; index + 1U < maximums.size(); ++index) {
        const auto &epoch = analysis->epochs[index];
        EXPECT_EQ(epoch.epoch_index, index + 1U);
        EXPECT_EQ(epoch.maximum_memory.kind, maximums[index].kind);
        EXPECT_EQ(epoch.maximum_memory.bytes, maximums[index].bytes);
        EXPECT_EQ(epoch.observation_count, 1U);
        EXPECT_EQ(epoch.termination_reason, TrendContinuityBreakReason::memory_maximum_changed);
    }
    const auto &last = analysis->epochs.back();
    EXPECT_EQ(last.epoch_index, maximums.size());
    EXPECT_EQ(last.maximum_memory.kind, finite);
    EXPECT_EQ(last.maximum_memory.bytes, 100U);
    EXPECT_EQ(last.observation_count, 2U);
    EXPECT_EQ(last.current_memory.first_bytes, bytes - 1U);
    EXPECT_EQ(last.current_memory.last_bytes, bytes);
    EXPECT_FALSE(last.termination_reason.has_value());
}

TEST(TrendAnalyzerTest, UnavailableCurrentClosesTheEpochAndAvailableCurrentStartsAnother) {
    TrendAnalyzer analyzer;
    const auto start = session_start.started_at;
    auto missing = observation();
    missing.current_memory_bytes = std::nullopt;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    accept_sample(analyzer, start, {missing});
    accept_sample(analyzer, start + 10s, {observation()});
    accept_sample(analyzer, start + 20s, {missing});
    accept_sample(analyzer, start + 30s, {missing});
    auto resumed = observation();
    resumed.current_memory_bytes = 0;
    accept_sample(analyzer, start + 40s, {resumed});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 2U);
    const auto &closed = analysis->epochs.front();
    EXPECT_EQ(closed.epoch_index, 1U);
    EXPECT_EQ(closed.observation_count, 1U);
    EXPECT_EQ(closed.last_observed_at, start + 10s);
    EXPECT_EQ(closed.termination_reason, TrendContinuityBreakReason::current_memory_unavailable);
    const auto &active = analysis->epochs.back();
    EXPECT_EQ(active.epoch_index, 2U);
    EXPECT_EQ(active.first_observed_at, start + 40s);
    EXPECT_EQ(active.current_memory.first_bytes, 0U);
    EXPECT_EQ(active.largest_gap_ms, 0U);
    EXPECT_FALSE(active.termination_reason.has_value());
}

TEST(TrendAnalyzerTest, NonIncreasingObservationsOpenNewEpochsInInputOrder) {
    TrendAnalyzer analyzer;
    const auto start = session_start.started_at;
    auto value = observation();
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    accept_sample(analyzer, start + 20s, {value});
    value.current_memory_bytes = 30U;
    value.maximum_memory.bytes = 200U;
    accept_sample(analyzer, start + 20s, {value});
    value.current_memory_bytes = 40U;
    accept_sample(analyzer, start + 10s, {value});
    value.current_memory_bytes = 50U;
    accept_sample(analyzer, start + 15s, {value});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 3U);
    for (std::size_t index = 0; index < 2U; ++index) {
        const auto &closed = analysis->epochs[index];
        EXPECT_EQ(closed.epoch_index, index + 1U);
        EXPECT_EQ(closed.first_observed_at, start + 20s);
        EXPECT_EQ(closed.termination_reason, TrendContinuityBreakReason::non_increasing_timestamp);
    }
    EXPECT_EQ(analysis->epochs[1].current_memory.first_bytes, 30U);
    EXPECT_EQ(analysis->epochs[1].maximum_memory.bytes, 200U);
    const auto &active = analysis->epochs.back();
    EXPECT_EQ(active.epoch_index, 3U);
    EXPECT_EQ(active.first_observed_at, start + 10s);
    EXPECT_EQ(active.last_observed_at, start + 15s);
    EXPECT_EQ(active.current_memory.first_bytes, 40U);
    EXPECT_EQ(active.current_memory.last_bytes, 50U);
    EXPECT_EQ(active.observation_count, 2U);
    EXPECT_EQ(active.largest_gap_ms, 5000U);
    EXPECT_FALSE(active.termination_reason.has_value());
}

TEST(TrendAnalyzerTest, MeasuresAbsenceBetweenActualObservationsAtTheSparseThreshold) {
    for (const auto excess : {0ms, 1ms}) {
        SCOPED_TRACE(excess.count());
        TrendAnalyzer analyzer;
        const auto start = session_start.started_at;
        ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
        accept_sample(analyzer, start + 20s, {observation()});
        accept_sample(analyzer, start + 30s, {});
        accept_sample(analyzer, start + 40s + excess, {observation()});
        ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

        const auto analysis = analyzer.finish();
        ASSERT_TRUE(analysis.has_value());
        ASSERT_EQ(analysis->epochs.size(), 1U);
        const auto &epoch = analysis->epochs.front();
        EXPECT_EQ(epoch.observation_count, 2U);
        EXPECT_EQ(epoch.largest_gap_ms, (20s + excess).count());
        EXPECT_EQ(epoch.quality,
                  excess == 0ms ? TrendEvidenceQuality::continuous : TrendEvidenceQuality::sparse);
        EXPECT_FALSE(epoch.termination_reason.has_value());
    }
}

TEST(TrendAnalyzerTest, UsesAdjacentObservationIntervalsAcrossAnAbsentSession) {
    for (const bool larger_interval_first : {false, true}) {
        SCOPED_TRACE(larger_interval_first);
        for (const auto excess : {0ms, 1ms}) {
            SCOPED_TRACE(excess.count());
            TrendAnalyzer analyzer;
            const auto start = session_start.started_at;
            const auto first_interval = larger_interval_first ? 20'000U : 10'000U;
            const auto next_interval = larger_interval_first ? 10'000U : 20'000U;
            ASSERT_TRUE(analyzer.begin_session({start, first_interval}).has_value());
            accept_sample(analyzer, start, {observation()});
            ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

            ASSERT_TRUE(analyzer.begin_session({start + 20s, 60'000U}).has_value());
            accept_sample(analyzer, start + 20s, {});
            ASSERT_TRUE(analyzer.end_session({.incomplete = true}).has_value());

            const auto resumed_at = start + 40s + excess;
            ASSERT_TRUE(analyzer.begin_session({resumed_at, next_interval}).has_value());
            accept_sample(analyzer, resumed_at, {observation()});
            accept_sample(analyzer, resumed_at + 10s, {observation()});
            ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

            const auto analysis = analyzer.finish();
            ASSERT_TRUE(analysis.has_value());
            ASSERT_EQ(analysis->epochs.size(), 1U);
            const auto &epoch = analysis->epochs.front();
            EXPECT_EQ(epoch.observation_count, 3U);
            EXPECT_EQ(epoch.observation_window_ms, (50s + excess).count());
            EXPECT_EQ(epoch.largest_gap_ms, (40s + excess).count());
            EXPECT_EQ(epoch.quality, excess == 0ms ? TrendEvidenceQuality::continuous
                                                   : TrendEvidenceQuality::sparse);
            EXPECT_FALSE(epoch.contains_incomplete_session);
        }
    }
}

TEST(TrendAnalyzerTest, RepresentsFullTimestampSpanAndLargeSamplingIntervals) {
    constexpr auto full_span = std::numeric_limits<std::uint64_t>::max();
    for (const auto interval : {full_span / 2U, full_span / 2U + 1U}) {
        SCOPED_TRACE(interval);
        TrendAnalyzer analyzer;
        ASSERT_TRUE(analyzer.begin_session({TrendTimePoint::min(), interval}).has_value());
        accept_sample(analyzer, TrendTimePoint::min(), {observation()});
        accept_sample(analyzer, TrendTimePoint::max(), {observation()});
        ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

        const auto analysis = analyzer.finish();
        ASSERT_TRUE(analysis.has_value());
        ASSERT_EQ(analysis->epochs.size(), 1U);
        const auto &epoch = analysis->epochs.front();
        EXPECT_EQ(epoch.observation_window_ms, full_span);
        EXPECT_EQ(epoch.largest_gap_ms, full_span);
        EXPECT_EQ(epoch.quality, interval == full_span / 2U ? TrendEvidenceQuality::sparse
                                                            : TrendEvidenceQuality::continuous);
    }
}

TEST(TrendAnalyzerTest, MarksAllAndOnlyEpochsThatReceivedIncompleteSessionData) {
    TrendAnalyzer analyzer;
    const auto start = session_start.started_at;
    auto other_path = observation();
    other_path.identity.cgroup_path = "/docker/second";
    auto other_id = observation();
    other_id.identity.container_id.back() = 'b';
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    accept_sample(analyzer, start, {other_id, other_path, observation()});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    ASSERT_TRUE(analyzer.begin_session({start + 10s, 10'000U}).has_value());
    other_id.current_memory_bytes = std::nullopt;
    accept_sample(analyzer, start + 10s, {observation(), other_id});
    auto changed_maximum = observation();
    changed_maximum.maximum_memory.bytes = 200U;
    accept_sample(analyzer, start + 20s, {changed_maximum});
    ASSERT_TRUE(analyzer.end_session({.incomplete = true}).has_value());

    ASSERT_TRUE(analyzer.begin_session({start + 30s, 10'000U}).has_value());
    accept_sample(analyzer, start + 30s, {other_path, changed_maximum});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 4U);
    const auto &epochs = analysis->epochs;
    EXPECT_EQ(epochs[0].identity, observation().identity);
    EXPECT_EQ(epochs[0].epoch_index, 1U);
    EXPECT_TRUE(epochs[0].contains_incomplete_session);
    EXPECT_EQ(epochs[0].termination_reason, TrendContinuityBreakReason::memory_maximum_changed);
    EXPECT_EQ(epochs[1].identity, observation().identity);
    EXPECT_EQ(epochs[1].epoch_index, 2U);
    EXPECT_TRUE(epochs[1].contains_incomplete_session);
    EXPECT_EQ(epochs[2].identity, other_path.identity);
    EXPECT_EQ(epochs[2].epoch_index, 1U);
    EXPECT_EQ(epochs[2].quality, TrendEvidenceQuality::sparse);
    EXPECT_FALSE(epochs[2].contains_incomplete_session);
    EXPECT_EQ(epochs[3].identity, other_id.identity);
    EXPECT_EQ(epochs[3].epoch_index, 1U);
    EXPECT_EQ(epochs[3].termination_reason, TrendContinuityBreakReason::current_memory_unavailable);
    EXPECT_FALSE(epochs[3].contains_incomplete_session);
    const auto &summary = analysis->summary;
    EXPECT_EQ(summary.epoch_count, 4U);
    EXPECT_EQ(summary.continuous_epoch_count, 3U);
    EXPECT_EQ(summary.sparse_epoch_count, 1U);
    EXPECT_EQ(summary.incomplete_epoch_count, 2U);
    EXPECT_EQ(summary.rate_available_epoch_count, 0U);
    EXPECT_EQ(summary.time_to_limit_available_epoch_count, 0U);
}

TEST(TrendAnalyzerTest, BoundsActiveEpochsAcrossSamples) {
    constexpr std::size_t limit = 4096U;
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    ASSERT_TRUE(analyzer.begin_sample(sample_start).has_value());
    auto value = observation();
    for (std::size_t index = 0; index < limit; ++index) {
        value.identity.cgroup_path = "/docker/container-" + std::to_string(index);
        ASSERT_TRUE(analyzer.observe(value).has_value()) << index;
    }
    ASSERT_TRUE(analyzer.end_sample().has_value());

    ASSERT_TRUE(analyzer.begin_sample({sample_start.captured_at + 10s}).has_value());
    ASSERT_TRUE(analyzer.observe(value).has_value());
    value.identity.cgroup_path = "/docker/overflow";
    expect_error(analyzer.observe(value), TrendAnalyzerErrorCode::resource_limit_exceeded);
    value.current_memory_bytes = std::nullopt;
    ASSERT_TRUE(analyzer.observe(value).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    EXPECT_EQ(analysis->epochs.size(), limit);
    EXPECT_EQ(analysis->summary.epoch_count, limit);
}

TEST(TrendAnalyzerTest, RejectsTheFirstEpochBeyondTheTotalLimitBeforeChangingState) {
    constexpr std::uint32_t limit = 4096U;
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    auto value = observation();
    auto captured_at = sample_start.captured_at;
    for (std::uint32_t index = 1U; index <= limit; ++index) {
        value.maximum_memory.bytes = index;
        accept_sample(analyzer, captured_at, {value});
        captured_at += 1ms;
    }

    ASSERT_TRUE(analyzer.begin_sample({captured_at}).has_value());
    auto changed_maximum = value;
    changed_maximum.maximum_memory.bytes = limit + 1U;
    expect_error(analyzer.observe(changed_maximum),
                 TrendAnalyzerErrorCode::resource_limit_exceeded);
    value.current_memory_bytes = 25U;
    ASSERT_TRUE(analyzer.observe(value).has_value());
    ASSERT_TRUE(analyzer.end_sample().has_value());
    ASSERT_TRUE(analyzer.end_session({.incomplete = true}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), limit);
    EXPECT_EQ(analysis->epochs.front().epoch_index, 1U);
    const auto &last = analysis->epochs.back();
    EXPECT_EQ(last.epoch_index, limit);
    EXPECT_FALSE(last.termination_reason.has_value());
    EXPECT_EQ(last.observation_count, 2U);
    EXPECT_EQ(last.current_memory.last_bytes, 25U);
    EXPECT_EQ(last.maximum_memory.bytes, limit);
    EXPECT_EQ(analysis->summary.incomplete_epoch_count, limit);
}

TEST(TrendAnalyzerTest, FitsAllObservationsAndReportsFiniteSignedRates) {
    struct RateCase {
        std::chrono::milliseconds middle_time;
        std::array<std::uint64_t, 3> bytes;
        double expected_rate;
    };
    constexpr std::array cases{
        RateCase{10s, {0, 100, 100}, 5.0},  RateCase{5s, {0, 100, 100}, 50.0 / 13.0},
        RateCase{10s, {100, 60, 20}, -4.0}, RateCase{10s, {100, 100, 100}, 0.0},
        RateCase{10s, {0, 0, 1}, 0.05},
    };
    for (const auto &rate_case : cases) {
        SCOPED_TRACE(rate_case.expected_rate);
        TrendAnalyzer analyzer;
        const auto start = session_start.started_at;
        ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
        auto value = observation();
        value.current_memory_bytes = rate_case.bytes[0];
        accept_sample(analyzer, start, {value});
        value.current_memory_bytes = rate_case.bytes[1];
        accept_sample(analyzer, start + rate_case.middle_time, {value});
        value.current_memory_bytes = rate_case.bytes[2];
        accept_sample(analyzer, start + 20s, {value});
        ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

        const auto analysis = analyzer.finish();
        ASSERT_TRUE(analysis.has_value());
        ASSERT_EQ(analysis->epochs.size(), 1U);
        const auto &current = analysis->epochs.front().current_memory;
        EXPECT_EQ(current.rate_status, TrendRateStatus::available);
        ASSERT_TRUE(current.growth_bytes_per_second.has_value());
        EXPECT_DOUBLE_EQ(*current.growth_bytes_per_second, rate_case.expected_rate);
        EXPECT_EQ(analysis->summary.rate_available_epoch_count, 1U);
    }
}

TEST(TrendAnalyzerTest, RequiresThreeObservationsBeforeCheckingTheTenSecondWindow) {
    struct EligibilityCase {
        bool has_middle_observation;
        std::chrono::milliseconds window;
        TrendRateStatus expected_status;
    };
    constexpr std::array cases{
        EligibilityCase{false, 9999ms, TrendRateStatus::insufficient_observations},
        EligibilityCase{false, 10s, TrendRateStatus::insufficient_observations},
        EligibilityCase{true, 9999ms, TrendRateStatus::insufficient_duration},
        EligibilityCase{true, 10s, TrendRateStatus::available},
    };
    for (const auto &eligibility : cases) {
        SCOPED_TRACE(eligibility.window.count());
        TrendAnalyzer analyzer;
        const auto start = session_start.started_at;
        ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
        accept_sample(analyzer, start, {observation()});
        if (eligibility.has_middle_observation) {
            accept_sample(analyzer, start + eligibility.window / 2, {observation()});
        }
        accept_sample(analyzer, start + eligibility.window, {observation()});
        ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

        const auto analysis = analyzer.finish();
        ASSERT_TRUE(analysis.has_value());
        ASSERT_EQ(analysis->epochs.size(), 1U);
        const auto &current = analysis->epochs.front().current_memory;
        const bool available = eligibility.expected_status == TrendRateStatus::available;
        EXPECT_EQ(current.rate_status, eligibility.expected_status);
        EXPECT_EQ(current.growth_bytes_per_second.has_value(), available);
        EXPECT_EQ(analysis->summary.rate_available_epoch_count, available ? 1U : 0U);
    }
}

TEST(TrendAnalyzerTest, PreservesByteSizedGrowthAtLargeTimestampsAndMemoryValues) {
    const auto start = TrendTimePoint::max() - 20s;
    constexpr auto maximum_bytes = std::numeric_limits<std::uint64_t>::max();
    TrendAnalyzer analyzer;
    ASSERT_TRUE(analyzer.begin_session({start, 10'000U}).has_value());
    auto value = observation();
    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unlimited};
    value.current_memory_bytes = maximum_bytes - 2U;
    accept_sample(analyzer, start, {value});
    value.current_memory_bytes = maximum_bytes - 1U;
    accept_sample(analyzer, start + 10s, {value});
    value.current_memory_bytes = maximum_bytes;
    accept_sample(analyzer, TrendTimePoint::max(), {value});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 1U);
    const auto &current = analysis->epochs.front().current_memory;
    EXPECT_EQ(current.rate_status, TrendRateStatus::available);
    ASSERT_TRUE(current.growth_bytes_per_second.has_value());
    EXPECT_DOUBLE_EQ(*current.growth_bytes_per_second, 0.1);
}

TEST(TrendAnalyzerTest, KeepsRateAvailableForSparseEpochWithUnavailableMaximum) {
    TrendAnalyzer analyzer;
    const auto start = session_start.started_at;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    auto value = observation();
    value.maximum_memory = {.kind = TrendMemoryMaximumKind::unavailable};
    value.current_memory_bytes = 30U;
    accept_sample(analyzer, start, {value});
    value.current_memory_bytes = 60U;
    accept_sample(analyzer, start + 30s, {value});
    value.current_memory_bytes = 70U;
    accept_sample(analyzer, start + 40s, {value});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 1U);
    const auto &epoch = analysis->epochs.front();
    EXPECT_EQ(epoch.quality, TrendEvidenceQuality::sparse);
    EXPECT_EQ(epoch.current_memory.rate_status, TrendRateStatus::available);
    ASSERT_TRUE(epoch.current_memory.growth_bytes_per_second.has_value());
    EXPECT_DOUBLE_EQ(*epoch.current_memory.growth_bytes_per_second, 1.0);
}

TEST(TrendAnalyzerTest, AccumulatesRatesIndependentlyOnBothSidesOfEpochBreaks) {
    TrendAnalyzer analyzer;
    const auto start = session_start.started_at;
    ASSERT_TRUE(analyzer.begin_session(session_start).has_value());
    auto value = observation();
    value.current_memory_bytes = 0U;
    accept_sample(analyzer, start, {value});
    value.current_memory_bytes = 50U;
    accept_sample(analyzer, start + 5s, {value});
    value.current_memory_bytes = 100U;
    accept_sample(analyzer, start + 10s, {value});

    value.maximum_memory.bytes = 1000U;
    value.current_memory_bytes = 300U;
    accept_sample(analyzer, start + 20s, {value});
    value.current_memory_bytes = 275U;
    accept_sample(analyzer, start + 25s, {value});
    value.current_memory_bytes = 250U;
    accept_sample(analyzer, start + 30s, {value});

    value.current_memory_bytes = std::nullopt;
    accept_sample(analyzer, start + 40s, {value});
    value.current_memory_bytes = 99U;
    accept_sample(analyzer, start + 50s, {value});
    value.current_memory_bytes = 500U;
    accept_sample(analyzer, start + 50s, {value});
    accept_sample(analyzer, start + 55s, {value});
    accept_sample(analyzer, start + 60s, {value});
    ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());

    const auto analysis = analyzer.finish();
    ASSERT_TRUE(analysis.has_value());
    ASSERT_EQ(analysis->epochs.size(), 4U);
    const auto &epochs = analysis->epochs;
    const std::array<std::optional<double>, 4> expected_rates{10.0, -5.0, std::nullopt, 0.0};
    for (std::size_t index = 0; index < expected_rates.size(); ++index) {
        SCOPED_TRACE(index);
        const auto &current = epochs[index].current_memory;
        const auto expected = expected_rates[index];
        EXPECT_EQ(current.rate_status, expected ? TrendRateStatus::available
                                                : TrendRateStatus::insufficient_observations);
        ASSERT_EQ(current.growth_bytes_per_second.has_value(), expected.has_value());
        if (expected) {
            EXPECT_DOUBLE_EQ(*current.growth_bytes_per_second, *expected);
        }
    }
    EXPECT_EQ(analysis->summary.rate_available_epoch_count, 3U);
    EXPECT_EQ(analysis->summary.time_to_limit_available_epoch_count, 0U);
}

} // namespace
} // namespace memoptima::trends
