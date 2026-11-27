#pragma once

#include <memoptima/forecast/memory_current_forecast.hpp>

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
struct SessionStart {
    std::uint64_t requested_interval_ms{1000U};
};
struct Observation {
    ContainerIdentity identity;
    std::optional<std::uint64_t> current_memory_bytes;
};
struct SessionEnd {
    bool incomplete;
};
struct ForecastEvaluationHorizon {
    std::chrono::milliseconds duration;
};

enum class ForecastEvaluationStatus {
    evaluated,
    excluded_incomplete_session,
    insufficient_observations,
    insufficient_observation_window,
    container_not_observed_continuously,
    historical_current_memory_unavailable,
    non_increasing_timestamps,
    horizon_below_sampling_interval,
    holdout_observation_unavailable,
    insufficient_origins,
    numeric_result_unavailable,
};

struct ForecastEvaluationRow {
    ContainerIdentity identity;
    std::uint32_t epoch_index;
    std::optional<std::chrono::milliseconds> horizon;
    std::uint64_t origin_count;
    ForecastEvaluationStatus status;
    std::optional<double> mean_absolute_error_bytes;
    std::optional<double> mean_persistence_absolute_error_bytes;
    std::optional<double> mean_signed_error_bytes;
    std::optional<double> relative_mean_absolute_error;
};

struct ForecastEvaluationHorizonSummary {
    std::chrono::milliseconds horizon;
    std::uint64_t contributing_epoch_count;
    std::uint64_t persistence_exact_model_exact_count;
    std::uint64_t persistence_exact_model_error_count;
    std::optional<double> mean_epoch_absolute_error_bytes;
    std::optional<double> mean_epoch_persistence_absolute_error_bytes;
    std::optional<double> mean_epoch_signed_error_bytes;
    std::optional<double> mean_epoch_relative_absolute_error;
    std::optional<double> skill;
};

struct ForecastEvaluationSummary {
    std::uint64_t row_count;
    std::uint64_t evaluated_epoch_horizon_count;
    std::map<ForecastEvaluationStatus, std::uint64_t> status_counts;
    std::vector<ForecastEvaluationHorizonSummary> horizons;
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
    explicit ForecastEvaluationAnalyzer(
        std::vector<ForecastEvaluationHorizon> horizons = default_horizons());
    [[nodiscard]] static std::vector<ForecastEvaluationHorizon> default_horizons();
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    begin_session(const SessionStart &start = {});
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    begin_sample(const SampleStart &sample);
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    observe(const Observation &observation);
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError> end_sample();
    [[nodiscard]] std::expected<void, ForecastEvaluationAnalyzerError>
    end_session(const SessionEnd &session);
    [[nodiscard]] std::expected<ForecastEvaluationReport, ForecastEvaluationAnalyzerError> finish();

  private:
    enum class State { between_sessions, in_session, in_sample, finished };
    struct Holdout {
        ForecastEvaluationTimePoint captured_at;
        std::uint64_t actual_memory_bytes;
    };
    struct Origin {
        std::uint64_t candidate_index{};
        ForecastEvaluationTimePoint captured_at{};
        std::uint64_t current_memory_bytes{};
        forecast::MemoryCurrentForecast model = forecast::fit_memory_current_forecast({});
        std::vector<std::optional<Holdout>> holdouts;
    };
    struct Epoch {
        ContainerIdentity identity;
        std::uint32_t index{};
        forecast::MemoryCurrentForecastAccumulator history;
        std::optional<ForecastEvaluationTimePoint> first_time;
        std::optional<ForecastEvaluationTimePoint> latest_time;
        std::uint64_t observation_count{};
        std::uint64_t candidate_count{};
        std::uint64_t decimation_stride{1U};
        bool contains_incomplete_session{};
        bool observed_in_session{};
        std::vector<Origin> origins;
    };
    void start_epoch(const Observation &observation);
    void append_observation(Epoch &epoch, std::uint64_t current_memory_bytes);
    void update_holdouts(Epoch &epoch, std::uint64_t current_memory_bytes);
    void retain_origin_if_eligible(Epoch &epoch, std::uint64_t current_memory_bytes);
    void decimate_origins(Epoch &epoch);
    void close_epoch(Epoch &&epoch, ForecastEvaluationStatus status);
    void close_absent_epochs();
    void append_epoch_rows(const Epoch &epoch, ForecastEvaluationStatus epoch_status);
    void summarize(ForecastEvaluationReport &report) const;
    State state_{State::between_sessions};
    std::vector<ForecastEvaluationHorizon> horizons_;
    std::uint64_t session_interval_ms_{};
    ForecastEvaluationTimePoint sample_time_{};
    std::set<ContainerIdentity> sample_identities_;
    std::map<ContainerIdentity, Epoch> active_epochs_;
    std::map<ContainerIdentity, std::uint32_t> next_epoch_indices_;
    std::vector<ForecastEvaluationRow> rows_;
    // Rows emitted during the open session must be invalidated if its terminal record is absent.
    std::vector<std::size_t> current_session_row_indices_;
    std::size_t completed_epoch_count_{};
};

} // namespace memoptima::forecast_evaluation
