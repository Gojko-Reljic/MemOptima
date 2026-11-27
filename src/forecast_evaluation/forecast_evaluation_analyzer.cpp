#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>

#include <algorithm>
#include <cmath>
#include <ranges>
#include <stdexcept>
#include <tuple>
#include <utility>

namespace memoptima::forecast_evaluation {
namespace {
constexpr std::size_t kMaximumSampleIdentities = 4096U;
constexpr std::size_t kMaximumActiveEpochs = 1024U;
constexpr std::size_t kMaximumResultEpochs = 4096U;
constexpr std::size_t kMaximumOriginsPerEpoch = 256U;
constexpr std::uint64_t kMinimumHistoryObservations = 3U;
constexpr auto kMinimumHistoryWindow = std::chrono::seconds{10};
constexpr long double kRelativeErrorDenominatorFloor = 1'048'576.0L;
constexpr long double kMinimumSkillRatio = 1e-6L;

[[nodiscard]] std::chrono::milliseconds elapsed(ForecastEvaluationTimePoint from,
                                                ForecastEvaluationTimePoint to) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(to - from);
}

[[nodiscard]] std::chrono::milliseconds holdout_tolerance(std::uint64_t interval_ms,
                                                          std::chrono::milliseconds horizon) {
    return std::min(std::chrono::milliseconds{interval_ms / 2U}, horizon / 10);
}

[[nodiscard]] std::chrono::milliseconds absolute_duration(std::chrono::milliseconds value) {
    return value < std::chrono::milliseconds::zero() ? -value : value;
}
} // namespace

ForecastEvaluationAnalyzer::ForecastEvaluationAnalyzer(
    std::vector<ForecastEvaluationHorizon> horizons)
    : horizons_(std::move(horizons)) {
    std::ranges::sort(horizons_, {}, &ForecastEvaluationHorizon::duration);
    horizons_.erase(std::unique(horizons_.begin(), horizons_.end(),
                                [](const auto &left, const auto &right) {
                                    return left.duration == right.duration;
                                }),
                    horizons_.end());
}

std::vector<ForecastEvaluationHorizon> ForecastEvaluationAnalyzer::default_horizons() {
    using namespace std::chrono_literals;
    return {{.duration = 1min}, {.duration = 5min}, {.duration = 15min}, {.duration = 1h}};
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::begin_session(const SessionStart &start) {
    if (state_ != State::between_sessions || start.requested_interval_ms == 0U) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail =
                "A session requires a positive interval and can begin only between sessions."});
    }
    session_interval_ms_ = start.requested_interval_ms;
    state_ = State::in_session;
    return {};
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::begin_sample(const SampleStart &sample) {
    if (state_ != State::in_session) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "A sample can begin only in an open session with no open sample."});
    }
    sample_time_ = sample.captured_at;
    sample_identities_.clear();
    state_ = State::in_sample;
    return {};
}

void ForecastEvaluationAnalyzer::start_epoch(const Observation &observation) {
    const auto [next, inserted] = next_epoch_indices_.try_emplace(observation.identity, 1U);
    Epoch epoch{.identity = observation.identity,
                .index = next->second,
                .history = {},
                .first_time = std::nullopt,
                .latest_time = std::nullopt,
                .observation_count = 0U,
                .candidate_count = 0U,
                .decimation_stride = 1U,
                .contains_incomplete_session = false,
                .observed_in_session = false,
                .origins = {}};
    ++next->second;
    append_observation(epoch, *observation.current_memory_bytes);
    active_epochs_.emplace(epoch.identity, std::move(epoch));
}

void ForecastEvaluationAnalyzer::append_observation(Epoch &epoch,
                                                    std::uint64_t current_memory_bytes) {
    epoch.history.add({.captured_at = sample_time_, .current_memory_bytes = current_memory_bytes});
    if (!epoch.first_time) {
        epoch.first_time = sample_time_;
    }
    epoch.latest_time = sample_time_;
    ++epoch.observation_count;
}

void ForecastEvaluationAnalyzer::update_holdouts(Epoch &epoch, std::uint64_t current_memory_bytes) {
    for (auto &origin : epoch.origins) {
        const auto actual_elapsed = elapsed(origin.captured_at, sample_time_);
        if (actual_elapsed <= std::chrono::milliseconds::zero()) {
            continue;
        }
        for (std::size_t index = 0U; index < horizons_.size(); ++index) {
            const auto horizon = horizons_[index].duration;
            if (horizon < std::chrono::milliseconds{session_interval_ms_}) {
                continue;
            }
            const auto tolerance = holdout_tolerance(session_interval_ms_, horizon);
            if (actual_elapsed < horizon - tolerance || actual_elapsed > horizon + tolerance) {
                continue;
            }
            const Holdout candidate{.captured_at = sample_time_,
                                    .actual_memory_bytes = current_memory_bytes};
            if (!origin.holdouts[index]) {
                origin.holdouts[index] = candidate;
                continue;
            }
            const auto previous_distance = absolute_duration(
                (origin.holdouts[index]->captured_at - origin.captured_at) - horizon);
            const auto candidate_distance =
                absolute_duration((candidate.captured_at - origin.captured_at) - horizon);
            if (candidate_distance < previous_distance ||
                (candidate_distance == previous_distance &&
                 candidate.captured_at < origin.holdouts[index]->captured_at)) {
                origin.holdouts[index] = candidate;
            }
        }
    }
}

void ForecastEvaluationAnalyzer::decimate_origins(Epoch &epoch) {
    while (epoch.origins.size() > kMaximumOriginsPerEpoch) {
        epoch.decimation_stride *= 2U;
        std::erase_if(epoch.origins, [&epoch](const Origin &origin) {
            return origin.candidate_index % epoch.decimation_stride != 0U;
        });
    }
}

void ForecastEvaluationAnalyzer::retain_origin_if_eligible(Epoch &epoch,
                                                           std::uint64_t current_memory_bytes) {
    if (epoch.observation_count < kMinimumHistoryObservations || !epoch.first_time ||
        !epoch.latest_time ||
        elapsed(*epoch.first_time, *epoch.latest_time) < kMinimumHistoryWindow) {
        return;
    }
    const auto candidate_index = epoch.candidate_count++;
    if (candidate_index % epoch.decimation_stride != 0U) {
        return;
    }
    epoch.origins.push_back({.candidate_index = candidate_index,
                             .captured_at = sample_time_,
                             .current_memory_bytes = current_memory_bytes,
                             .model = epoch.history.finish(),
                             .holdouts = std::vector<std::optional<Holdout>>(horizons_.size())});
    decimate_origins(epoch);
}

void ForecastEvaluationAnalyzer::append_epoch_rows(const Epoch &epoch,
                                                   ForecastEvaluationStatus epoch_status) {
    const auto row_limit = kMaximumResultEpochs * horizons_.size();
    if (rows_.size() >= row_limit) {
        throw std::length_error("The forecast evaluation result row limit was reached.");
    }
    if (epoch_status != ForecastEvaluationStatus::evaluated) {
        rows_.push_back({.identity = epoch.identity,
                         .epoch_index = epoch.index,
                         .horizon = std::nullopt,
                         .origin_count = 0U,
                         .status = epoch.contains_incomplete_session
                                       ? ForecastEvaluationStatus::excluded_incomplete_session
                                       : epoch_status,
                         .mean_absolute_error_bytes = std::nullopt,
                         .mean_persistence_absolute_error_bytes = std::nullopt,
                         .mean_signed_error_bytes = std::nullopt,
                         .relative_mean_absolute_error = std::nullopt});
        return;
    }
    for (std::size_t index = 0U; index < horizons_.size(); ++index) {
        const auto horizon = horizons_[index].duration;
        ForecastEvaluationRow row{.identity = epoch.identity,
                                  .epoch_index = epoch.index,
                                  .horizon = horizon,
                                  .origin_count = 0U,
                                  .status = ForecastEvaluationStatus::evaluated,
                                  .mean_absolute_error_bytes = std::nullopt,
                                  .mean_persistence_absolute_error_bytes = std::nullopt,
                                  .mean_signed_error_bytes = std::nullopt,
                                  .relative_mean_absolute_error = std::nullopt};
        if (horizon < std::chrono::milliseconds{session_interval_ms_}) {
            row.status = ForecastEvaluationStatus::horizon_below_sampling_interval;
            rows_.push_back(std::move(row));
            continue;
        }
        long double absolute_error{};
        long double persistence_error{};
        long double signed_error{};
        long double relative_denominator{};
        bool numeric_failure{};
        for (const auto &origin : epoch.origins) {
            if (!origin.holdouts[index]) {
                continue;
            }
            const auto projection = origin.model.project_after(
                elapsed(origin.captured_at, origin.holdouts[index]->captured_at));
            if (projection.availability !=
                    forecast::MemoryCurrentProjectionAvailability::available ||
                !projection.current_memory_bytes) {
                numeric_failure = true;
                continue;
            }
            const auto predicted = static_cast<long double>(*projection.current_memory_bytes);
            const auto actual =
                static_cast<long double>(origin.holdouts[index]->actual_memory_bytes);
            absolute_error += std::abs(predicted - actual);
            persistence_error +=
                std::abs(static_cast<long double>(origin.current_memory_bytes) - actual);
            signed_error += predicted - actual;
            relative_denominator += std::max(static_cast<long double>(origin.current_memory_bytes),
                                             kRelativeErrorDenominatorFloor);
            ++row.origin_count;
        }
        if (epoch.contains_incomplete_session) {
            row.status = ForecastEvaluationStatus::excluded_incomplete_session;
        } else if (row.origin_count == 0U) {
            row.status = numeric_failure
                             ? ForecastEvaluationStatus::numeric_result_unavailable
                             : ForecastEvaluationStatus::holdout_observation_unavailable;
        } else if (row.origin_count < 3U) {
            row.status = ForecastEvaluationStatus::insufficient_origins;
        } else {
            const auto count = static_cast<long double>(row.origin_count);
            row.mean_absolute_error_bytes = static_cast<double>(absolute_error / count);
            row.mean_persistence_absolute_error_bytes =
                static_cast<double>(persistence_error / count);
            row.mean_signed_error_bytes = static_cast<double>(signed_error / count);
            row.relative_mean_absolute_error =
                static_cast<double>(absolute_error / relative_denominator);
        }
        rows_.push_back(std::move(row));
    }
}

void ForecastEvaluationAnalyzer::close_epoch(Epoch &&epoch, ForecastEvaluationStatus status) {
    if (completed_epoch_count_ >= kMaximumResultEpochs) {
        throw std::length_error(
            "The forecast evaluation epoch limit of 4096 was reached; split the manifest.");
    }
    append_epoch_rows(epoch, status);
    ++completed_epoch_count_;
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::observe(const Observation &observation) {
    if (state_ != State::in_sample) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "An observation requires an open sample."});
    }
    if (observation.identity.container_id.empty() || observation.identity.cgroup_path.empty()) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_observation,
            .detail = "A container observation requires a non-empty identity."});
    }
    if (!sample_identities_.insert(observation.identity).second) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::duplicate_observation,
            .detail = "The container identity already has an observation in this sample."});
    }
    if (sample_identities_.size() > kMaximumSampleIdentities) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = "The sample identity limit was reached."});
    }
    try {
        auto active = active_epochs_.find(observation.identity);
        if (!observation.current_memory_bytes) {
            if (active != active_epochs_.end()) {
                close_epoch(std::move(active->second),
                            ForecastEvaluationStatus::historical_current_memory_unavailable);
                active_epochs_.erase(active);
            } else {
                const auto [next, inserted] =
                    next_epoch_indices_.try_emplace(observation.identity, 1U);
                Epoch epoch{.identity = observation.identity,
                            .index = next->second++,
                            .history = {},
                            .first_time = std::nullopt,
                            .latest_time = std::nullopt,
                            .observation_count = 0U,
                            .candidate_count = 0U,
                            .decimation_stride = 1U,
                            .contains_incomplete_session = false,
                            .observed_in_session = false,
                            .origins = {}};
                close_epoch(std::move(epoch),
                            ForecastEvaluationStatus::historical_current_memory_unavailable);
            }
            return {};
        }
        if (active == active_epochs_.end()) {
            if (active_epochs_.size() >= kMaximumActiveEpochs) {
                return std::unexpected(ForecastEvaluationAnalyzerError{
                    .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
                    .detail = "The active epoch limit of 1024 was reached."});
            }
            start_epoch(observation);
            active_epochs_.find(observation.identity)->second.observed_in_session = true;
            return {};
        }
        auto &epoch = active->second;
        epoch.observed_in_session = true;
        if (sample_time_ <= *epoch.latest_time) {
            close_epoch(std::move(epoch), ForecastEvaluationStatus::non_increasing_timestamps);
            active_epochs_.erase(active);
            start_epoch(observation);
            active_epochs_.find(observation.identity)->second.observed_in_session = true;
            return {};
        }
        update_holdouts(epoch, *observation.current_memory_bytes);
        append_observation(epoch, *observation.current_memory_bytes);
        retain_origin_if_eligible(epoch, *observation.current_memory_bytes);
        return {};
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what()});
    }
}

void ForecastEvaluationAnalyzer::close_absent_epochs() {
    std::vector<ContainerIdentity> absent;
    for (const auto &[identity, epoch] : active_epochs_) {
        (void)epoch;
        if (!sample_identities_.contains(identity))
            absent.push_back(identity);
    }
    for (const auto &identity : absent) {
        auto active = active_epochs_.find(identity);
        close_epoch(std::move(active->second),
                    ForecastEvaluationStatus::container_not_observed_continuously);
        active_epochs_.erase(active);
    }
}

std::expected<void, ForecastEvaluationAnalyzerError> ForecastEvaluationAnalyzer::end_sample() {
    if (state_ != State::in_sample) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Ending a sample requires an open sample."});
    }
    try {
        close_absent_epochs();
        state_ = State::in_session;
        return {};
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what()});
    }
}

std::expected<void, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::end_session(const SessionEnd &session) {
    if (state_ != State::in_session) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Ending a session requires an open session with no open sample."});
    }
    try {
        for (auto &[identity, epoch] : active_epochs_) {
            (void)identity;
            if (session.incomplete) {
                epoch.contains_incomplete_session = true;
            }
            const auto status =
                epoch.observation_count < kMinimumHistoryObservations
                    ? ForecastEvaluationStatus::insufficient_observations
                : (!epoch.first_time || !epoch.latest_time ||
                   elapsed(*epoch.first_time, *epoch.latest_time) < kMinimumHistoryWindow)
                    ? ForecastEvaluationStatus::insufficient_observation_window
                    : ForecastEvaluationStatus::evaluated;
            close_epoch(std::move(epoch), status);
        }
        active_epochs_.clear();
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what()});
    }
    state_ = State::between_sessions;
    return {};
}

void ForecastEvaluationAnalyzer::summarize(ForecastEvaluationReport &report) const {
    for (const auto &row : report.rows) {
        ++report.summary.status_counts[row.status];
        if (row.status == ForecastEvaluationStatus::evaluated)
            ++report.summary.evaluated_epoch_horizon_count;
    }
    report.summary.row_count = report.rows.size();
    for (const auto &horizon : horizons_) {
        long double model_mae{};
        long double persistence_mae{};
        long double bias{};
        long double relative_mae{};
        long double log_relative_mae{};
        std::uint64_t contributing_epochs{};
        std::uint64_t skill_epochs{};
        std::uint64_t persistence_exact_model_exact{};
        std::uint64_t persistence_exact_model_error{};
        for (const auto &row : report.rows) {
            if (row.horizon != horizon.duration ||
                row.status != ForecastEvaluationStatus::evaluated) {
                continue;
            }
            ++contributing_epochs;
            model_mae += *row.mean_absolute_error_bytes;
            persistence_mae += *row.mean_persistence_absolute_error_bytes;
            bias += *row.mean_signed_error_bytes;
            relative_mae += *row.relative_mean_absolute_error;
            if (*row.mean_persistence_absolute_error_bytes == 0.0) {
                if (*row.mean_absolute_error_bytes == 0.0) {
                    ++persistence_exact_model_exact;
                } else {
                    ++persistence_exact_model_error;
                }
                continue;
            }
            const auto ratio =
                std::max(static_cast<long double>(*row.mean_absolute_error_bytes) /
                             static_cast<long double>(*row.mean_persistence_absolute_error_bytes),
                         kMinimumSkillRatio);
            log_relative_mae += std::log(ratio);
            ++skill_epochs;
        }
        ForecastEvaluationHorizonSummary summary{
            .horizon = horizon.duration,
            .contributing_epoch_count = contributing_epochs,
            .persistence_exact_model_exact_count = persistence_exact_model_exact,
            .persistence_exact_model_error_count = persistence_exact_model_error,
            .mean_epoch_absolute_error_bytes = std::nullopt,
            .mean_epoch_persistence_absolute_error_bytes = std::nullopt,
            .mean_epoch_signed_error_bytes = std::nullopt,
            .mean_epoch_relative_absolute_error = std::nullopt,
            .skill = std::nullopt};
        if (contributing_epochs != 0U) {
            const auto divisor = static_cast<long double>(contributing_epochs);
            summary.mean_epoch_absolute_error_bytes = static_cast<double>(model_mae / divisor);
            summary.mean_epoch_persistence_absolute_error_bytes =
                static_cast<double>(persistence_mae / divisor);
            summary.mean_epoch_signed_error_bytes = static_cast<double>(bias / divisor);
            summary.mean_epoch_relative_absolute_error =
                static_cast<double>(relative_mae / divisor);
        }
        if (skill_epochs != 0U) {
            summary.skill = static_cast<double>(
                1.0L - std::exp(log_relative_mae / static_cast<long double>(skill_epochs)));
        }
        report.summary.horizons.push_back(std::move(summary));
    }
}

std::expected<ForecastEvaluationReport, ForecastEvaluationAnalyzerError>
ForecastEvaluationAnalyzer::finish() {
    if (state_ != State::between_sessions) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::invalid_event_sequence,
            .detail = "Finishing evaluation requires closed sessions and an unfinished analyzer."});
    }
    try {
        for (auto &[identity, epoch] : active_epochs_) {
            (void)identity;
            const auto status =
                epoch.observation_count < kMinimumHistoryObservations
                    ? ForecastEvaluationStatus::insufficient_observations
                : (!epoch.first_time || !epoch.latest_time ||
                   elapsed(*epoch.first_time, *epoch.latest_time) < kMinimumHistoryWindow)
                    ? ForecastEvaluationStatus::insufficient_observation_window
                    : ForecastEvaluationStatus::evaluated;
            close_epoch(std::move(epoch), status);
        }
        active_epochs_.clear();
    } catch (const std::length_error &error) {
        return std::unexpected(ForecastEvaluationAnalyzerError{
            .code = ForecastEvaluationAnalyzerErrorCode::resource_limit_exceeded,
            .detail = error.what()});
    }
    ForecastEvaluationReport report{.summary = {}, .rows = std::move(rows_)};
    std::ranges::sort(report.rows, {}, [](const ForecastEvaluationRow &row) {
        return std::tuple{row.identity, row.epoch_index,
                          row.horizon.value_or(std::chrono::milliseconds::zero())};
    });
    summarize(report);
    state_ = State::finished;
    return report;
}

} // namespace memoptima::forecast_evaluation
