#include <memoptima/forecast/memory_limit_forecast.hpp>

namespace memoptima::forecast {
namespace {

[[nodiscard]] MemoryLimitForecast unavailable(MemoryLimitForecastAvailability availability,
                                              std::optional<double> growth = std::nullopt) {
    return {
        .availability = availability,
        .growth_bytes_per_second = growth,
        .estimated_time_to_memory_max = std::nullopt,
    };
}

[[nodiscard]] MemoryLimitForecastAvailability
map_current_memory_availability(const MemoryCurrentForecastAvailability availability) {
    switch (availability) {
    case MemoryCurrentForecastAvailability::available:
        return MemoryLimitForecastAvailability::available;
    case MemoryCurrentForecastAvailability::insufficient_observations:
        return MemoryLimitForecastAvailability::insufficient_observations;
    case MemoryCurrentForecastAvailability::insufficient_observation_window:
        return MemoryLimitForecastAvailability::insufficient_observation_window;
    case MemoryCurrentForecastAvailability::non_increasing_timestamps:
        return MemoryLimitForecastAvailability::non_increasing_timestamps;
    case MemoryCurrentForecastAvailability::numeric_result_unavailable:
        return MemoryLimitForecastAvailability::numeric_result_unavailable;
    }
    return MemoryLimitForecastAvailability::numeric_result_unavailable;
}

} // namespace

void MemoryLimitForecastAccumulator::add(const MemoryLimitObservation &observation) {
    if (!memory_max_bytes_.has_value()) {
        memory_max_bytes_ = observation.memory_max_bytes;
    } else if (observation.memory_max_bytes != *memory_max_bytes_) {
        memory_max_changed_ = true;
    }

    latest_current_memory_bytes_ = observation.current_memory_bytes;
    current_memory_accumulator_.add({.captured_at = observation.captured_at,
                                     .current_memory_bytes = observation.current_memory_bytes});
}

MemoryLimitForecast MemoryLimitForecastAccumulator::finish() const {
    const MemoryCurrentForecast current_memory_forecast = current_memory_accumulator_.finish();
    if (current_memory_forecast.availability() != MemoryCurrentForecastAvailability::available) {
        return unavailable(map_current_memory_availability(current_memory_forecast.availability()));
    }
    if (memory_max_changed_) {
        return unavailable(MemoryLimitForecastAvailability::unstable_memory_max);
    }
    if (latest_current_memory_bytes_ >= *memory_max_bytes_) {
        return unavailable(MemoryLimitForecastAvailability::already_at_or_above_memory_max);
    }

    const double growth = *current_memory_forecast.growth_bytes_per_second();
    if (growth <= 0.0) {
        return unavailable(MemoryLimitForecastAvailability::non_positive_growth, growth);
    }

    const auto duration = current_memory_forecast.estimate_duration_to(latest_current_memory_bytes_,
                                                                       *memory_max_bytes_);
    if (duration.availability == MemoryCurrentDurationAvailability::duration_out_of_range) {
        return unavailable(MemoryLimitForecastAvailability::time_to_limit_out_of_range, growth);
    }
    if (duration.availability != MemoryCurrentDurationAvailability::available ||
        !duration.duration_ms) {
        return unavailable(MemoryLimitForecastAvailability::numeric_result_unavailable, growth);
    }

    return {
        .availability = MemoryLimitForecastAvailability::available,
        .growth_bytes_per_second = growth,
        .estimated_time_to_memory_max =
            std::chrono::milliseconds{
                static_cast<std::chrono::milliseconds::rep>(*duration.duration_ms)},
    };
}

MemoryLimitForecast forecast_memory_limit(std::span<const MemoryLimitObservation> observations) {
    MemoryLimitForecastAccumulator accumulator;
    for (const MemoryLimitObservation &observation : observations) {
        accumulator.add(observation);
    }
    return accumulator.finish();
}

} // namespace memoptima::forecast
