#include <memoptima/forecast/memory_limit_forecast.hpp>

#include <cmath>
#include <limits>

namespace memoptima::forecast {
namespace {

constexpr std::size_t kMinimumObservationCount = 3;
constexpr auto kMinimumObservationWindow = std::chrono::seconds{10};

[[nodiscard]] MemoryLimitForecast unavailable(MemoryLimitForecastAvailability availability,
                                              std::optional<double> growth = std::nullopt) {
    return {
        .availability = availability,
        .growth_bytes_per_second = growth,
        .estimated_time_to_memory_max = std::nullopt,
    };
}

} // namespace

void MemoryLimitForecastAccumulator::add(const MemoryLimitObservation &observation) {
    if (!first_captured_at_.has_value()) {
        first_captured_at_ = observation.captured_at;
        memory_max_bytes_ = observation.memory_max_bytes;
    } else {
        if (observation.captured_at <= *last_captured_at_) {
            invalid_series_reason_ = MemoryLimitForecastAvailability::non_increasing_timestamps;
        }
        if (observation.memory_max_bytes != *memory_max_bytes_) {
            invalid_series_reason_ = MemoryLimitForecastAvailability::unstable_memory_max;
        }
    }

    const auto offset = observation.captured_at - *first_captured_at_;
    const long double time_seconds =
        static_cast<long double>(offset.count()) / std::chrono::milliseconds{1}.count() / 1000.0L;
    sum_time_seconds_ += time_seconds;
    sum_memory_bytes_ += static_cast<long double>(observation.current_memory_bytes);
    sum_time_squared_ += time_seconds * time_seconds;
    sum_time_memory_ += time_seconds * static_cast<long double>(observation.current_memory_bytes);
    latest_current_memory_bytes_ = observation.current_memory_bytes;
    last_captured_at_ = observation.captured_at;
    ++observation_count_;
}

MemoryLimitForecast MemoryLimitForecastAccumulator::finish() const {
    if (observation_count_ < kMinimumObservationCount) {
        return unavailable(MemoryLimitForecastAvailability::insufficient_observations);
    }
    if (invalid_series_reason_.has_value()) {
        return unavailable(*invalid_series_reason_);
    }

    const auto observation_window = *last_captured_at_ - *first_captured_at_;
    if (observation_window < kMinimumObservationWindow) {
        return unavailable(MemoryLimitForecastAvailability::insufficient_observation_window);
    }
    if (latest_current_memory_bytes_ >= *memory_max_bytes_) {
        return unavailable(MemoryLimitForecastAvailability::already_at_or_above_memory_max);
    }

    const long double count = static_cast<long double>(observation_count_);
    const long double covariance =
        sum_time_memory_ - (sum_time_seconds_ * sum_memory_bytes_ / count);
    const long double time_variance =
        sum_time_squared_ - (sum_time_seconds_ * sum_time_seconds_ / count);
    const double growth = static_cast<double>(covariance / time_variance);
    if (!std::isfinite(growth) || growth <= 0.0) {
        return unavailable(MemoryLimitForecastAvailability::non_positive_growth, growth);
    }

    const long double bytes_remaining =
        static_cast<long double>(*memory_max_bytes_ - latest_current_memory_bytes_);
    const long double milliseconds_to_limit =
        bytes_remaining / static_cast<long double>(growth) * 1000.0L;
    if (!std::isfinite(milliseconds_to_limit) ||
        milliseconds_to_limit >
            static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return unavailable(MemoryLimitForecastAvailability::time_to_limit_out_of_range, growth);
    }

    return {
        .availability = MemoryLimitForecastAvailability::available,
        .growth_bytes_per_second = growth,
        .estimated_time_to_memory_max =
            std::chrono::milliseconds{static_cast<std::int64_t>(std::ceil(milliseconds_to_limit))},
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
