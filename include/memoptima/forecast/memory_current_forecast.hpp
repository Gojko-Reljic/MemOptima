#pragma once

#include <chrono>
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

class MemoryCurrentForecast {
  public:
    [[nodiscard]] MemoryCurrentForecastAvailability availability() const;

    [[nodiscard]] std::optional<double> growth_bytes_per_second() const;

    // Projects forward from the latest fitted observation at a non-negative elapsed duration.
    [[nodiscard]] MemoryCurrentProjection project_after(std::chrono::milliseconds elapsed) const;

  private:
    friend MemoryCurrentForecast
    fit_memory_current_forecast(std::span<const MemoryCurrentObservation> observations);

    MemoryCurrentForecast(MemoryCurrentForecastAvailability availability,
                          std::optional<double> growth_bytes_per_second,
                          long double intercept_bytes, long double latest_offset_seconds);

    MemoryCurrentForecastAvailability availability_;
    std::optional<double> growth_bytes_per_second_;
    long double intercept_bytes_{};
    long double latest_offset_seconds_{};
};

// Fits the shared linear memory.current model used by forecasting and forecast evaluation.
[[nodiscard]] MemoryCurrentForecast
fit_memory_current_forecast(std::span<const MemoryCurrentObservation> observations);

} // namespace memoptima::forecast
