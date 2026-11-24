#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace memoptima::forecast_evaluation {
namespace {

constexpr std::size_t kMaximumSampleIdentities = 4096U;
constexpr std::size_t kMaximumActiveEpochs = 4096U;
constexpr std::size_t kMaximumRows = 4096U;
constexpr std::uint64_t kMinimumHistoryObservations = 3U;
constexpr std::uint64_t kMinimumHistoryWindowMs = 10'000U;

[[nodiscard]] std::uint64_t elapsed_ms(ForecastEvaluationTimePoint first,
                                       ForecastEvaluationTimePoint last) {
    return static_cast<std::uint64_t>(last.time_since_epoch().count()) -
           static_cast<std::uint64_t>(first.time_since_epoch().count());
}

[[nodiscard]] bool is_eligible_status(ForecastEvaluationStatus status) {
    return status == ForecastEvaluationStatus::evaluated ||
           status == ForecastEvaluationStatus::holdout_observation_unavailable ||
           status == ForecastEvaluationStatus::numeric_result_unavailable;
}

} // namespace

std::expected<void, ForecastEvaluationAnalyzerError> ForecastEvaluationAnalyzer::begin_session() {
    if (state_ != State::between_sessions) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "A session can begin only between sessions.",
        });
    }
    current_session_row_indices_.clear();
    state_ = State::in_session;
    return {};
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::begin_sample(const SampleStart &sample) {
    if (state_ != State::in_session) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "A sample can begin only in an open session with no open sample.",
        });
    }
    sample_time_ = sample.captured_at;
    sample_identities_.clear();
    state_ = State::in_sample;
    return {};
}

void ForecastEvaluationAnalyzer::start_epoch(const Observation &observation) {
    const auto [next, inserted] = next_epoch_indices_.try_emplace(observation.identity, 1U);
    const auto index = next->second;
    ++next->second;
    Epoch epoch{};
    epoch.identity = observation.identity;
    epoch.index = index;
    append_history(epoch, *observation.current_memory_bytes);
    active_epochs_.emplace(epoch.identity, std::move(epoch));
}

void ForecastEvaluationAnalyzer::append_history(Epoch &epoch, std::uint64_t current_memory_bytes) {
    const auto captured_at = sample_time_;
    if (epoch.history_count == 0U) {
        epoch.first_history_time = captured_at;
    }
    const auto first = *epoch.first_history_time;
    const auto time_seconds =
        static_cast<long double>(elapsed_ms(first, captured_at)) / static_cast<long double>(1000U);
    const auto memory_bytes = static_cast<long double>(current_memory_bytes);
    ++epoch.history_count;
    const auto count = static_cast<long double>(epoch.history_count);
    const auto time_delta = time_seconds - epoch.mean_time_seconds;
    const auto memory_delta = memory_bytes - epoch.mean_memory_bytes;
    epoch.mean_time_seconds += time_delta / count;
    epoch.mean_memory_bytes += memory_delta / count;
    const auto weight = static_cast<long double>(epoch.history_count - 1U) / count;
    epoch.time_variance_sum += weight * time_delta * time_delta;
    epoch.time_memory_covariance_sum += weight * time_delta * memory_delta;
    epoch.last_history_time = captured_at;
    epoch.latest_time = captured_at;

    if (epoch.history_count >= kMinimumHistoryObservations &&
        elapsed_ms(first, captured_at) >= kMinimumHistoryWindowMs) {
        epoch.candidate_ready = true;
    }
}

void ForecastEvaluationAnalyzer::emit_epoch(Epoch &epoch, ForecastEvaluationStatus status) {
    if (epoch.emitted) {
        return;
    }
    if (rows_.size() >= kMaximumRows) {
        throw std::length_error("The forecast evaluation row limit was reached.");
    }
    const auto first = epoch.first_history_time;
    const auto last = epoch.last_history_time;
    const auto window = first && last ? elapsed_ms(*first, *last) : 0U;
    if (epoch.contains_incomplete_session) {
        status = ForecastEvaluationStatus::excluded_incomplete_session;
    }
    rows_.push_back({.identity = epoch.identity,
                     .epoch_index = epoch.index,
                     .history_first_timestamp = first,
                     .history_last_timestamp = last,
                     .holdout_timestamp = std::nullopt,
                     .history_observation_count = epoch.history_count,
                     .history_window_ms = window,
                     .status = status,
                     .predicted_current_bytes = std::nullopt,
                     .actual_current_bytes = std::nullopt,
                     .signed_error_bytes = std::nullopt,
                     .absolute_error_bytes = std::nullopt});
    epoch.emitted = true;
    epoch.emitted_row_index = rows_.size() - 1U;
    current_session_row_indices_.push_back(*epoch.emitted_row_index);
}

void ForecastEvaluationAnalyzer::emit_evaluated_epoch(Epoch &epoch,
                                                      std::uint64_t actual_memory_bytes) {
    if (epoch.emitted) {
        return;
    }
    if (rows_.size() >= kMaximumRows) {
        throw std::length_error("The forecast evaluation row limit was reached.");
    }
    const auto first = *epoch.first_history_time;
    const auto holdout_seconds =
        static_cast<long double>(elapsed_ms(first, sample_time_)) / static_cast<long double>(1000U);
    const auto slope = epoch.time_memory_covariance_sum / epoch.time_variance_sum;
    const auto intercept = epoch.mean_memory_bytes - slope * epoch.mean_time_seconds;
    const auto projection = std::fma(slope, holdout_seconds, intercept);
    const auto double_max = static_cast<long double>(std::numeric_limits<double>::max());
    if (!std::isfinite(slope) || !std::isfinite(intercept) || !std::isfinite(projection) ||
        std::abs(projection) > double_max) {
        emit_epoch(epoch, ForecastEvaluationStatus::numeric_result_unavailable);
        return;
    }
    const auto predicted = static_cast<double>(std::max(projection, 0.0L));
    const auto actual = static_cast<double>(actual_memory_bytes);
    const auto signed_error = predicted - actual;
    const auto absolute_error = std::abs(signed_error);
    const auto last = *epoch.last_history_time;
    rows_.push_back(
        {.identity = epoch.identity,
         .epoch_index = epoch.index,
         .history_first_timestamp = first,
         .history_last_timestamp = last,
         .holdout_timestamp = sample_time_,
         .history_observation_count = epoch.history_count,
         .history_window_ms = elapsed_ms(first, last),
         .status = epoch.contains_incomplete_session
                       ? ForecastEvaluationStatus::excluded_incomplete_session
                       : ForecastEvaluationStatus::evaluated,
         .predicted_current_bytes =
             epoch.contains_incomplete_session ? std::nullopt : std::optional<double>{predicted},
         .actual_current_bytes = epoch.contains_incomplete_session
                                     ? std::nullopt
                                     : std::optional<std::uint64_t>{actual_memory_bytes},
         .signed_error_bytes =
             epoch.contains_incomplete_session ? std::nullopt : std::optional<double>{signed_error},
         .absolute_error_bytes = epoch.contains_incomplete_session
                                     ? std::nullopt
                                     : std::optional<double>{absolute_error}});
    epoch.emitted = true;
    epoch.emitted_row_index = rows_.size() - 1U;
    current_session_row_indices_.push_back(*epoch.emitted_row_index);
}

void ForecastEvaluationAnalyzer::emit_missing_memory(const ContainerIdentity &identity) {
    if (rows_.size() >= kMaximumRows) {
        throw std::length_error("The forecast evaluation row limit was reached.");
    }
    const auto [next, inserted] = next_epoch_indices_.try_emplace(identity, 1U);
    const auto index = next->second;
    ++next->second;
    rows_.push_back({.identity = identity,
                     .epoch_index = index,
                     .history_first_timestamp = std::nullopt,
                     .history_last_timestamp = std::nullopt,
                     .holdout_timestamp = std::nullopt,
                     .history_observation_count = 0U,
                     .history_window_ms = 0U,
                     .status = ForecastEvaluationStatus::historical_current_memory_unavailable,
                     .predicted_current_bytes = std::nullopt,
                     .actual_current_bytes = std::nullopt,
                     .signed_error_bytes = std::nullopt,
                     .absolute_error_bytes = std::nullopt});
    current_session_row_indices_.push_back(rows_.size() - 1U);
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::observe(const Observation &observation) {
    if (state_ != State::in_sample) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "An observation requires an open sample.",
        });
    }
    if (observation.identity.container_id.empty() || observation.identity.cgroup_path.empty()) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_observation,
            .detail = "A container observation requires a non-empty identity.",
        });
    }
    if (sample_identities_.contains(observation.identity)) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::duplicate_observation,
            .detail = "The container identity already has an observation in this sample.",
        });
    }
    if (sample_identities_.size() >= kMaximumSampleIdentities) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = "The sample identity limit was reached.",
        });
    }
    sample_identities_.insert(observation.identity);

    auto active = active_epochs_.find(observation.identity);
    try {
        if (!observation.current_memory_bytes) {
            if (active != active_epochs_.end()) {
                emit_epoch(active->second,
                           active->second.candidate_ready
                               ? ForecastEvaluationStatus::holdout_observation_unavailable
                               : ForecastEvaluationStatus::historical_current_memory_unavailable);
                active_epochs_.erase(active);
            } else {
                emit_missing_memory(observation.identity);
            }
            return {};
        }
        if (active == active_epochs_.end()) {
            if (active_epochs_.size() >= kMaximumActiveEpochs) {
                return std::unexpected(ForecastEvaluationAnalyzerError{
                    .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
                    .detail = "The active epoch limit was reached.",
                });
            }
            start_epoch(observation);
            active_epochs_.find(observation.identity)->second.observed_in_session = true;
            return {};
        }
        auto &epoch = active->second;
        epoch.observed_in_session = true;
        if (sample_time_ <= epoch.latest_time) {
            emit_epoch(epoch, ForecastEvaluationStatus::non_increasing_timestamps);
            active_epochs_.erase(active);
            if (active_epochs_.size() >= kMaximumActiveEpochs) {
                return std::unexpected(ForecastEvaluationAnalyzerError{
                    .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
                    .detail = "The active epoch limit was reached.",
                });
            }
            start_epoch(observation);
            active_epochs_.find(observation.identity)->second.observed_in_session = true;
            return {};
        }
        if (epoch.candidate_ready) {
            emit_evaluated_epoch(epoch, *observation.current_memory_bytes);
            return {};
        }
        append_history(epoch, *observation.current_memory_bytes);
        return {};
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what(),
        });
    }
}

void ForecastEvaluationAnalyzer::close_absent_epochs() {
    std::vector<ContainerIdentity> absent;
    for (const auto &[identity, epoch] : active_epochs_) {
        (void)epoch;
        if (!sample_identities_.contains(identity)) {
            absent.push_back(identity);
        }
    }
    for (const auto &identity : absent) {
        auto active = active_epochs_.find(identity);
        if (!active->second.emitted) {
            emit_epoch(active->second,
                       active->second.candidate_ready
                           ? ForecastEvaluationStatus::holdout_observation_unavailable
                           : ForecastEvaluationStatus::container_not_observed_continuously);
        }
        active_epochs_.erase(active);
    }
}

std::expected<void, ForecastEvaluationAnalyzerError> ForecastEvaluationAnalyzer::end_sample() {
    if (state_ != State::in_sample) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Ending a sample requires an open sample.",
        });
    }
    try {
        close_absent_epochs();
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what(),
        });
    }
    state_ = State::in_session;
    return {};
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::end_session(const SessionEnd &session) {
    if (state_ != State::in_session) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Ending a session requires an open session with no open sample.",
        });
    }
    if (session.incomplete) {
        for (const auto index : current_session_row_indices_) {
            auto &row = rows_[index];
            row.status = ForecastEvaluationStatus::excluded_incomplete_session;
            row.predicted_current_bytes.reset();
            row.actual_current_bytes.reset();
            row.signed_error_bytes.reset();
            row.absolute_error_bytes.reset();
        }
    }
    for (auto &[identity, epoch] : active_epochs_) {
        (void)identity;
        if (session.incomplete && epoch.observed_in_session) {
            epoch.contains_incomplete_session = true;
        }
        epoch.observed_in_session = false;
    }
    state_ = State::between_sessions;
    return {};
}

void ForecastEvaluationAnalyzer::summarize(ForecastEvaluationReport &report) const {
    long double total_absolute_error{};
    long double total_signed_error{};
    for (const auto &row : report.rows) {
        ++report.summary.status_counts[row.status];
        if (row.status == ForecastEvaluationStatus::excluded_incomplete_session) {
            ++report.summary.excluded_incomplete_session_count;
        }
        if (is_eligible_status(row.status)) {
            ++report.summary.eligible_forecast_count;
        }
        if (row.status == ForecastEvaluationStatus::evaluated) {
            ++report.summary.evaluated_forecast_count;
            total_absolute_error += *row.absolute_error_bytes;
            total_signed_error += *row.signed_error_bytes;
        }
    }
    report.summary.row_count = report.rows.size();
    if (report.summary.eligible_forecast_count != 0U) {
        report.summary.coverage = static_cast<double>(report.summary.evaluated_forecast_count) /
                                  static_cast<double>(report.summary.eligible_forecast_count);
    }
    if (report.summary.evaluated_forecast_count != 0U) {
        const auto count = static_cast<long double>(report.summary.evaluated_forecast_count);
        report.summary.mean_absolute_error_bytes =
            static_cast<double>(total_absolute_error / count);
        report.summary.mean_signed_error_bytes = static_cast<double>(total_signed_error / count);
    }
}

std::expected<ForecastEvaluationReport, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::finish() {
    if (state_ != State::between_sessions) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Finishing evaluation requires closed sessions and an unfinished analyzer.",
        });
    }
    try {
        for (auto &[identity, epoch] : active_epochs_) {
            (void)identity;
            if (!epoch.emitted) {
                emit_epoch(epoch, epoch.candidate_ready
                                      ? ForecastEvaluationStatus::holdout_observation_unavailable
                                  : epoch.history_count >= kMinimumHistoryObservations
                                      ? ForecastEvaluationStatus::insufficient_observation_window
                                      : ForecastEvaluationStatus::insufficient_observations);
            }
        }
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what(),
        });
    }

    ForecastEvaluationReport report{};
    report.rows = std::move(rows_);
    std::ranges::sort(report.rows, {}, [](const ForecastEvaluationRow &row) {
        return std::pair{row.identity, row.epoch_index};
    });
    summarize(report);
    active_epochs_.clear();
    next_epoch_indices_.clear();
    state_ = State::finished;
    return report;
}

} // namespace memoptima::forecast_evaluation
