#include <memoptima/trends/trend_analyzer.hpp>

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <utility>

namespace memoptima::trends {
namespace {

constexpr std::size_t kMaximumSampleIdentities = 4096U;
constexpr std::size_t kMaximumActiveEpochs = 4096U;
constexpr std::size_t kMaximumEpochResults = 4096U;

std::optional<TrendContinuityBreakReason> continuity_break(const TrendEpochResult &result,
                                                           const TrendObservation &observation,
                                                           TrendTimePoint captured_at) {
    if (!observation.current_memory_bytes) {
        return TrendContinuityBreakReason::current_memory_unavailable;
    }
    if (captured_at <= result.last_observed_at) {
        return TrendContinuityBreakReason::non_increasing_timestamp;
    }
    if (observation.maximum_memory.kind != result.maximum_memory.kind ||
        observation.maximum_memory.bytes != result.maximum_memory.bytes) {
        return TrendContinuityBreakReason::memory_maximum_changed;
    }
    return std::nullopt;
}

std::uint64_t elapsed_ms(TrendTimePoint first, TrendTimePoint last) {
    // Unsigned subtraction represents the full span between ordered signed timestamps.
    return static_cast<std::uint64_t>(last.time_since_epoch().count()) -
           static_cast<std::uint64_t>(first.time_since_epoch().count());
}

} // namespace

std::expected<void, TrendAnalyzerError>
TrendAnalyzer::begin_session(const TrendSessionStart &start) {
    if (state_ != State::between_sessions) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                                                  "A session can begin only between sessions."});
    }

    session_interval_ms_ = start.requested_interval_ms;
    state_ = State::in_session;
    return {};
}

std::expected<void, TrendAnalyzerError>
TrendAnalyzer::begin_sample(const TrendSampleStart &sample) {
    if (state_ != State::in_session) {
        return std::unexpected(
            TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                               "A sample can begin only in an open session with no open sample."});
    }

    sample_time_ = sample.captured_at;
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

    if (sample_identities_.contains(observation.identity)) {
        return std::unexpected(TrendAnalyzerError{
            TrendAnalyzerErrorCode::duplicate_observation,
            "The container identity already has an observation in this sample."});
    }
    if (sample_identities_.size() >= kMaximumSampleIdentities) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::resource_limit_exceeded,
                                                  "The sample identity limit was reached."});
    }

    const auto epoch_count = epochs_.size();
    const auto latest = latest_epoch_by_identity_.find(observation.identity);
    const auto previous_index =
        latest == latest_epoch_by_identity_.end() ? epoch_count : latest->second;
    const bool has_previous = previous_index != epoch_count;
    const bool has_active = has_previous && !epochs_[previous_index].result.termination_reason;
    const auto reason =
        has_active ? continuity_break(epochs_[previous_index].result, observation, sample_time_)
                   : std::nullopt;
    const bool starts_epoch = observation.current_memory_bytes && (!has_active || reason);

    if (starts_epoch && !has_active && active_epoch_count_ >= kMaximumActiveEpochs) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::resource_limit_exceeded,
                                                  "The active epoch limit was reached."});
    }
    if (starts_epoch && epoch_count >= kMaximumEpochResults) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::resource_limit_exceeded,
                                                  "The total epoch result limit was reached."});
    }

    const auto sample_entry = sample_identities_.insert(observation.identity).first;
    if (starts_epoch) {
        const auto index = has_previous ? epochs_[previous_index].result.epoch_index + 1U : 1U;
        try {
            start_epoch(observation, index);
        } catch (...) {
            sample_identities_.erase(sample_entry);
            throw;
        }
        ++active_epoch_count_;
    }

    if (reason) {
        epochs_[previous_index].result.termination_reason = reason;
        --active_epoch_count_;
    } else if (has_active) {
        append_observation(epochs_[previous_index], *observation.current_memory_bytes);
    }

    return {};
}

void TrendAnalyzer::start_epoch(const TrendObservation &observation, std::uint32_t index) {
    const auto bytes = *observation.current_memory_bytes;
    const auto slot = epochs_.size();
    epochs_.push_back({
        .result = {.identity = observation.identity,
                   .epoch_index = index,
                   .first_observed_at = sample_time_,
                   .last_observed_at = sample_time_,
                   .observation_count = 1,
                   .observation_window_ms = 0,
                   .largest_gap_ms = 0,
                   .quality = TrendEvidenceQuality::continuous,
                   .contains_incomplete_session = false,
                   .termination_reason = std::nullopt,
                   .current_memory = {.first_bytes = bytes,
                                      .last_bytes = bytes,
                                      .rate_status = TrendRateStatus::insufficient_observations,
                                      .growth_bytes_per_second = std::nullopt},
                   .maximum_memory = observation.maximum_memory,
                   .time_to_limit = {.status = TrendTimeToLimitStatus::rate_unavailable,
                                     .estimated_duration_ms = std::nullopt}},
        .last_interval_ms = session_interval_ms_,
        .observed_in_session = true,
    });
    try {
        latest_epoch_by_identity_.insert_or_assign(observation.identity, slot);
    } catch (...) {
        epochs_.pop_back();
        throw;
    }
}

void TrendAnalyzer::append_observation(Epoch &epoch, std::uint64_t current_bytes) {
    auto &result = epoch.result;
    const auto gap = elapsed_ms(result.last_observed_at, sample_time_);
    const auto interval = std::max(epoch.last_interval_ms, session_interval_ms_);
    // Compare against twice the interval without overflowing for a large declared interval.
    if (gap > interval && gap - interval > interval) {
        result.quality = TrendEvidenceQuality::sparse;
    }
    result.largest_gap_ms = std::max(result.largest_gap_ms, gap);
    result.last_observed_at = sample_time_;
    result.observation_window_ms = elapsed_ms(result.first_observed_at, sample_time_);
    result.current_memory.last_bytes = current_bytes;
    ++result.observation_count;
    epoch.last_interval_ms = session_interval_ms_;
    epoch.observed_in_session = true;
}

std::expected<void, TrendAnalyzerError> TrendAnalyzer::end_sample() {
    if (state_ != State::in_sample) {
        return std::unexpected(TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                                                  "Ending a sample requires an open sample."});
    }

    // An absent identity keeps its last observation so its next one measures the full gap.
    sample_identities_.clear();
    state_ = State::in_session;
    return {};
}

std::expected<void, TrendAnalyzerError> TrendAnalyzer::end_session(const TrendSessionEnd &end) {
    if (state_ != State::in_session) {
        return std::unexpected(
            TrendAnalyzerError{TrendAnalyzerErrorCode::invalid_event_sequence,
                               "Ending a session requires an open session with no open sample."});
    }

    for (auto &epoch : epochs_) {
        if (end.incomplete && epoch.observed_in_session) {
            epoch.result.contains_incomplete_session = true;
        }
        epoch.observed_in_session = false;
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

    TrendAnalysis analysis{};
    analysis.epochs.reserve(epochs_.size());
    for (auto &epoch : epochs_) {
        if (epoch.result.quality == TrendEvidenceQuality::sparse) {
            ++analysis.summary.sparse_epoch_count;
        }
        if (epoch.result.contains_incomplete_session) {
            ++analysis.summary.incomplete_epoch_count;
        }
        analysis.epochs.push_back(std::move(epoch.result));
    }
    analysis.summary.epoch_count = analysis.epochs.size();
    analysis.summary.continuous_epoch_count =
        analysis.summary.epoch_count - analysis.summary.sparse_epoch_count;
    std::ranges::sort(analysis.epochs, {}, [](const TrendEpochResult &epoch) {
        return std::tie(epoch.identity, epoch.epoch_index);
    });

    latest_epoch_by_identity_.clear();
    epochs_.clear();
    state_ = State::finished;
    return analysis;
}

} // namespace memoptima::trends
