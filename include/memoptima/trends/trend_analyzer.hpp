#pragma once

#include <memoptima/trends/evidence_session_catalog.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace memoptima::trends {

using TrendTimePoint = std::chrono::sys_time<std::chrono::milliseconds>;

enum class TrendMemoryMaximumKind {
    finite,
    unlimited,
    unavailable,
};

struct TrendMemoryMaximum {
    TrendMemoryMaximumKind kind;
    std::uint64_t bytes{};
};

struct TrendSessionStart {
    TrendTimePoint started_at;
    std::uint64_t requested_interval_ms;
};

struct TrendSampleStart {
    TrendTimePoint captured_at;
};

struct TrendObservation {
    TrendContainerIdentity identity;
    std::optional<std::uint64_t> current_memory_bytes;
    TrendMemoryMaximum maximum_memory;
};

struct TrendSessionEnd {
    bool incomplete;
};

enum class TrendAnalyzerErrorCode {
    invalid_event_sequence,
    duplicate_observation,
    invalid_observation,
    resource_limit_exceeded,
};

struct TrendAnalyzerError {
    TrendAnalyzerErrorCode code;
    std::string detail;
};

enum class TrendRateStatus {
    available,
    insufficient_observations,
    insufficient_duration,
};

enum class TrendTimeToLimitStatus {
    available,
    rate_unavailable,
    non_positive_growth,
    sparse_evidence,
    unlimited_maximum,
    unavailable_maximum,
    already_at_or_above_maximum,
    duration_out_of_range,
};

enum class TrendEvidenceQuality {
    continuous,
    sparse,
};

enum class TrendContinuityBreakReason {
    current_memory_unavailable,
    memory_maximum_changed,
    non_increasing_timestamp,
};

struct TrendCurrentMemoryResult {
    std::uint64_t first_bytes;
    std::uint64_t last_bytes;
    TrendRateStatus rate_status;
    std::optional<double> growth_bytes_per_second;
};

struct TrendTimeToLimitResult {
    TrendTimeToLimitStatus status;
    std::optional<std::uint64_t> estimated_duration_ms;
};

struct TrendEpochResult {
    TrendContainerIdentity identity;
    std::uint32_t epoch_index;
    TrendTimePoint first_observed_at;
    TrendTimePoint last_observed_at;
    std::uint64_t observation_count;
    std::uint64_t observation_window_ms;
    std::uint64_t largest_gap_ms;
    TrendEvidenceQuality quality;
    bool contains_incomplete_session;
    std::optional<TrendContinuityBreakReason> termination_reason;
    TrendCurrentMemoryResult current_memory;
    TrendMemoryMaximum maximum_memory;
    TrendTimeToLimitResult time_to_limit;
};

struct TrendAnalysisSummary {
    std::uint64_t epoch_count;
    std::uint64_t continuous_epoch_count;
    std::uint64_t sparse_epoch_count;
    std::uint64_t incomplete_epoch_count;
    std::uint64_t rate_available_epoch_count;
    std::uint64_t time_to_limit_available_epoch_count;
};

struct TrendAnalysis {
    TrendAnalysisSummary summary;
    std::vector<TrendEpochResult> epochs;
};

class TrendAnalyzer {
  public:
    [[nodiscard]] std::expected<void, TrendAnalyzerError>
    begin_session(const TrendSessionStart &start);

    [[nodiscard]] std::expected<void, TrendAnalyzerError>
    begin_sample(const TrendSampleStart &sample);

    [[nodiscard]] std::expected<void, TrendAnalyzerError>
    observe(const TrendObservation &observation);

    [[nodiscard]] std::expected<void, TrendAnalyzerError> end_sample();

    [[nodiscard]] std::expected<void, TrendAnalyzerError> end_session(const TrendSessionEnd &end);

    // Returns epoch metadata and current-memory rates. Time-to-limit calculations are deferred.
    [[nodiscard]] std::expected<TrendAnalysis, TrendAnalyzerError> finish();

  private:
    enum class State {
        between_sessions,
        in_session,
        in_sample,
        finished,
    };

    struct Epoch {
        TrendEpochResult result;
        std::uint64_t last_interval_ms;
        bool observed_in_session;
        long double mean_time_seconds{};
        long double mean_memory_offset_bytes{};
        long double time_variance_sum{};
        long double time_memory_covariance_sum{};
    };

    void start_epoch(const TrendObservation &observation, std::uint32_t index);
    void append_observation(Epoch &epoch, std::uint64_t current_bytes);
    static void update_rate_statistics(Epoch &epoch);
    static void finalize_rate(Epoch &epoch);

    State state_{State::between_sessions};
    std::uint64_t session_interval_ms_{};
    TrendTimePoint sample_time_{};
    std::set<TrendContainerIdentity> sample_identities_;
    std::map<TrendContainerIdentity, std::size_t> latest_epoch_by_identity_;
    std::vector<Epoch> epochs_;
    std::size_t active_epoch_count_{};
};

} // namespace memoptima::trends
