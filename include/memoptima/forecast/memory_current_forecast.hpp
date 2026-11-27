#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace memoptima::forecast {

using MemoryCurrentForecastTimePoint = std::chrono::sys_time<std::chrono::milliseconds>;

struct MemoryCurrentObservation {
    MemoryCurrentForecastTimePoint captured_at;
    std::uint64_t current_memory_bytes;
};

enum class MemoryCurrentForecastAvailability {
    available,
    insufficient_observations,
    insufficient_observation_window,
    non_increasing_timestamps,
    numeric_result_unavailable,
};

enum class MemoryCurrentProjectionAvailability {
    available,
    forecast_unavailable,
    numeric_result_unavailable,
};

struct MemoryCurrentProjection {
    MemoryCurrentProjectionAvailability availability;
    std::optional<std::uint64_t> current_memory_bytes;
};

enum class MemoryCurrentDurationAvailability {
    available,
    forecast_unavailable,
    non_positive_growth,
    target_not_above_current,
    duration_out_of_range,
};

struct MemoryCurrentDurationEstimate {
    MemoryCurrentDurationAvailability availability;
    std::optional<std::uint64_t> duration_ms;
};

class MemoryCurrentForecastAccumulator;

class MemoryCurrentForecast {
  public:
    [[nodiscard]] MemoryCurrentForecastAvailability availability() const;

    [[nodiscard]] std::optional<double> growth_bytes_per_second() const;

    // Projects forward from the latest fitted observation at a non-negative elapsed duration.
    [[nodiscard]] MemoryCurrentProjection project_after(std::chrono::milliseconds elapsed) const;

    // Estimates a floor-rounded duration from current bytes to a strictly greater target.
    [[nodiscard]] MemoryCurrentDurationEstimate
    estimate_duration_to(std::uint64_t current_memory_bytes,
                         std::uint64_t target_memory_bytes) const;

  private:
    friend MemoryCurrentForecast
    fit_memory_current_forecast(std::span<const MemoryCurrentObservation> observations);
    friend class MemoryCurrentForecastAccumulator;

    MemoryCurrentForecast(MemoryCurrentForecastAvailability availability,
                          std::optional<double> growth_bytes_per_second,
                          long double memory_baseline_bytes, long double intercept_offset_bytes,
                          long double latest_offset_seconds, long double time_variance_sum,
                          long double time_memory_covariance_sum);

    MemoryCurrentForecastAvailability availability_;
    std::optional<double> growth_bytes_per_second_;
    long double memory_baseline_bytes_{};
    long double intercept_offset_bytes_{};
    long double latest_offset_seconds_{};
    long double time_variance_sum_{};
    long double time_memory_covariance_sum_{};
};

class MemoryCurrentForecastAccumulator {
  public:
    void add(const MemoryCurrentObservation &observation);

    [[nodiscard]] MemoryCurrentForecast finish() const;

  private:
    std::size_t observation_count_{};
    std::optional<MemoryCurrentForecastAvailability> invalid_series_reason_;
    std::optional<MemoryCurrentForecastTimePoint> first_captured_at_;
    std::optional<MemoryCurrentForecastTimePoint> last_captured_at_;
    std::optional<std::uint64_t> first_current_memory_bytes_;
    long double mean_time_seconds_{};
    long double mean_memory_bytes_{};
    long double time_variance_sum_{};
    long double time_memory_covariance_sum_{};
};

// Fits the shared linear memory.current model used by forecasting and forecast evaluation.
[[nodiscard]] MemoryCurrentForecast
fit_memory_current_forecast(std::span<const MemoryCurrentObservation> observations);

} // namespace memoptima::forecast
