#include <memoptima/trends/trend_analyzer.hpp>

#include <cstddef>

namespace memoptima::trends {
namespace {

constexpr std::size_t kMaximumSampleIdentities = 4096U;

} // namespace

std::expected<void, TrendAnalyzerError> TrendAnalyzer::begin_session(const TrendSessionStart &) {
    if (state_ != State::between_sessions) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                                                  "A session can begin only between sessions."});
    }

    state_ = State::in_session;
    return {};
}

std::expected<void, TrendAnalyzerError> TrendAnalyzer::begin_sample(const TrendSampleStart &) {
    if (state_ != State::in_session) {
        return std::unexpected(
            TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                               "A sample can begin only in an open session with no open sample."});
    }

    state_ = State::in_sample;
    return {};
}

std::expected<void, TrendAnalyzerError>
TrendAnalyzer::observe(const TrendObservation &observation) {
    if (state_ != State::in_sample) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                                                  "An observation requires an open sample."});
    }

    const auto [kind, bytes] = observation.maximum_memory;
    const bool canonical_maximum = kind == TrendMemoryMaximumKind::finite ||
                                   (bytes == 0 && (kind == TrendMemoryMaximumKind::unlimited ||
                                                   kind == TrendMemoryMaximumKind::unavailable));
    if (!canonical_maximum) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_observation,
                                                  "The maximum memory value is not canonical."});
    }

    if (sample_identities_.size() >= kMaximumSampleIdentities &&
        !sample_identities_.contains(observation.identity)) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::resource_limit_exceeded,
                                                  "The sample identity limit was reached."});
    }

    if (!sample_identities_.insert(observation.identity).second) {
        return std::unexpected(TrendAnalyzerError{
            TrendAnalyzerErrorCode::duplicate_observation,
            "The container identity already has an observation in this sample."});
    }

    return {};
}

std::expected<void, TrendAnalyzerError> TrendAnalyzer::end_sample() {
    if (state_ != State::in_sample) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                                                  "Ending a sample requires an open sample."});
    }

    sample_identities_.clear();
    state_ = State::in_session;
    return {};
}

std::expected<void, TrendAnalyzerError> TrendAnalyzer::end_session(const TrendSessionEnd &) {
    if (state_ != State::in_session) {
        return std::unexpected(
            TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                               "Ending a session requires an open session with no open sample."});
    }

    state_ = State::between_sessions;
    return {};
}

std::expected<TrendAnalysis, TrendAnalyzerError> TrendAnalyzer::finish() {
    if (state_ != State::between_sessions) {
        return std::unexpected(TrendAnalyzerError{
            TrendAnalyzerErrorCode::invalid_event_sequence,
            "Finishing analysis requires closed sessions and an unfinished analyzer."});
    }

    state_ = State::finished;
    return TrendAnalysis{};
}

} // namespace memoptima::trends
