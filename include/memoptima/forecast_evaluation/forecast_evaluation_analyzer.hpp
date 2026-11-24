#pragma once

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace memoptima::forecast_evaluation {

using ForecastEvaluationTimePoint = std::chrono::sys_time<std::chrono::milliseconds>;

struct ContainerIdentity {
    std::string container_id;
    std::string cgroup_path;

    [[nodiscard]] auto operator<=>(const ContainerIdentity &) const = default;
};

struct SampleStart {
    ForecastEvaluationTimePoint captured_at;
};

struct Observation {
    ContainerIdentity identity;
    std::optional<std::uint64_t> current_memory_bytes;
};

struct SessionEnd {
    bool incomplete;
};

enum class ForecastEvaluationStatus {
    evaluated,
    excluded_incomplete_session,
    insufficient_observations,
    insufficient_observation_window,
    container_not_observed_continuously,
    historical_current_memory_unavailable,
    non_increasing_timestamps,
    holdout_observation_unavailable,
    numeric_result_unavailable,
};

struct ForecastEvaluationRow {
    ContainerIdentity identity;
    std::uint32_t epoch_index;
    std::optional<ForecastEvaluationTimePoint> history_first_timestamp;
    std::optional<ForecastEvaluationTimePoint> history_last_timestamp;
    std::optional<ForecastEvaluationTimePoint> holdout_timestamp;
    std::uint64_t history_observation_count;
    std::uint64_t history_window_ms;
    ForecastEvaluationStatus status;
    std::optional<double> predicted_current_bytes;
    std::optional<std::uint64_t> actual_current_bytes;
    std::optional<double> signed_error_bytes;
    std::optional<double> absolute_error_bytes;
};

struct ForecastEvaluationSummary {
    std::uint64_t row_count;
    std::uint64_t eligible_forecast_count;
    std::uint64_t evaluated_forecast_count;
    std::uint64_t excluded_incomplete_session_count;
    std::map<ForecastEvaluationStatus, std::uint64_t> status_counts;
    std::optional<double> coverage;
    std::optional<double> mean_absolute_error_bytes;
    std::optional<double> mean_signed_error_bytes;
};

struct ForecastEvaluationReport {
    ForecastEvaluationSummary summary;
    std::vector<ForecastEvaluationRow> rows;
};

enum class ForecastEvaluationAnalyzerErrorCode {
    invalid_event_sequence,
    duplicate_observation,
    invalid_observation,
    resource_limit_exceeded,
};

struct ForecastEvaluationAnalyzerError {
    ForecastEvaluationAnalyzerErrorCode code;
    std::string detail;
};

class ForecastEvaluationAnalyzer {
  public:
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError> begin_session();

    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    begin_sample(const SampleStart &sample);

    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    observe(const Observation &observation);

    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError> end_sample();

    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    end_session(const SessionEnd &session);

    [[nodiscard]] std::expected<ForecastEvaluationReport, ForecastEvaluationAnalyzerError> finish();

  private:
    enum class State {
        between_sessions,
        in_session,
        in_sample,
        finished,
    };

    struct Epoch {
        ContainerIdentity identity;
        std::uint32_t index{};
        std::optional<ForecastEvaluationTimePoint> first_history_time;
        std::optional<ForecastEvaluationTimePoint> last_history_time;
        std::uint64_t history_count{};
        ForecastEvaluationTimePoint latest_time{};
        bool candidate_ready{};
        bool contains_incomplete_session{};
        bool observed_in_session{};
        bool emitted{};
        std::optional<std::size_t> emitted_row_index;
        long double mean_time_seconds{};
        long double mean_memory_bytes{};
        long double time_variance_sum{};
        long double time_memory_covariance_sum{};
    };

    void start_epoch(const Observation &observation);
    void append_history(Epoch &epoch, std::uint64_t current_memory_bytes);
    void emit_epoch(Epoch &epoch, ForecastEvaluationStatus status);
    void emit_evaluated_epoch(Epoch &epoch, std::uint64_t actual_memory_bytes);
    void emit_missing_memory(const ContainerIdentity &identity);
    void close_absent_epochs();
    void summarize(ForecastEvaluationReport &report) const;

    State state_{State::between_sessions};
    ForecastEvaluationTimePoint sample_time_{};
    std::set<ContainerIdentity> sample_identities_;
    std::map<ContainerIdentity, Epoch> active_epochs_;
    std::map<ContainerIdentity, std::uint32_t> next_epoch_indices_;
    std::vector<ForecastEvaluationRow> rows_;
    std::vector<std::size_t> current_session_row_indices_;
};

} // namespace memoptima::forecast_evaluation
