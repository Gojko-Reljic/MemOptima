#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>

namespace memoptima::forecast {

struct MemoryLimitObservation {
    std::chrono::sys_time<std::chrono::milliseconds> captured_at;
    std::uint64_t current_memory_bytes;
    std::uint64_t memory_max_bytes;
};

enum class MemoryLimitForecastAvailability {
    available,
    insufficient_observations,
    insufficient_observation_window,
    non_increasing_timestamps,
    unstable_memory_max,
    already_at_or_above_memory_max,
    non_positive_growth,
    time_to_limit_out_of_range,
};

struct MemoryLimitForecast {
    MemoryLimitForecastAvailability availability;
    std::optional<double> growth_bytes_per_second;
    std::optional<std::chrono::milliseconds> estimated_time_to_memory_max;
};

// Estimates time to a stable finite cgroup memory.max from sufficient timestamped observations.
[[nodiscard]] MemoryLimitForecast
forecast_memory_limit(std::span<const MemoryLimitObservation> observations);

} // namespace memoptima::forecast
