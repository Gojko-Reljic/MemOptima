#include <memoptima/forecast/memory_current_forecast.hpp>

#include <cmath>
#include <limits>

namespace memoptima::forecast {
namespace {

constexpr std::size_t kMinimumObservationCount = 3U;
constexpr auto kMinimumObservationWindow = std::chrono::seconds{10};

} // namespace

MemoryCurrentForecast::MemoryCurrentForecast(const MemoryCurrentForecastAvailability availability,
                                             const std::optional<double> growth_bytes_per_second,
                                             const long double memory_baseline_bytes,
                                             const long double intercept_offset_bytes,
                                             const long double latest_offset_seconds,
                                             const long double time_variance_sum,
                                             const long double time_memory_covariance_sum)
    : availability_(availability), growth_bytes_per_second_(growth_bytes_per_second),
      memory_baseline_bytes_(memory_baseline_bytes),
      intercept_offset_bytes_(intercept_offset_bytes),
      latest_offset_seconds_(latest_offset_seconds), time_variance_sum_(time_variance_sum),
      time_memory_covariance_sum_(time_memory_covariance_sum) {}

MemoryCurrentForecastAvailability MemoryCurrentForecast::availability() const {
    return availability_;
}

std::optional<double> MemoryCurrentForecast::growth_bytes_per_second() const {
    return growth_bytes_per_second_;
}

MemoryCurrentProjection
MemoryCurrentForecast::project_after(const std::chrono::milliseconds elapsed) const {
    if (availability_ != MemoryCurrentForecastAvailability::available) {
        return {MemoryCurrentProjectionAvailability::forecast_unavailable, std::nullopt};
    }
    if (elapsed < std::chrono::milliseconds::zero()) {
        return {MemoryCurrentProjectionAvailability::numeric_result_unavailable, std::nullopt};
    }

    const long double elapsed_seconds =
        static_cast<long double>(elapsed.count()) / static_cast<long double>(1000U);
    const long double projected =
        std::fma(static_cast<long double>(*growth_bytes_per_second_),
                 latest_offset_seconds_ + elapsed_seconds, intercept_offset_bytes_) +
        memory_baseline_bytes_;
    const long double clamped = std::max(projected, 0.0L);
    const long double rounded = std::round(clamped);
    const long double upper_bound = std::ldexp(1.0L, std::numeric_limits<std::uint64_t>::digits);
    if (!std::isfinite(projected) || !std::isfinite(clamped) || !std::isfinite(rounded) ||
        rounded >= upper_bound) {
        return {MemoryCurrentProjectionAvailability::numeric_result_unavailable, std::nullopt};
    }

    return {MemoryCurrentProjectionAvailability::available, static_cast<std::uint64_t>(rounded)};
}

MemoryCurrentDurationEstimate
MemoryCurrentForecast::estimate_duration_to(const std::uint64_t current_memory_bytes,
                                            const std::uint64_t target_memory_bytes) const {
    if (availability_ != MemoryCurrentForecastAvailability::available) {
        return {MemoryCurrentDurationAvailability::forecast_unavailable, std::nullopt};
    }
    if (target_memory_bytes <= current_memory_bytes) {
        return {MemoryCurrentDurationAvailability::target_not_above_current, std::nullopt};
    }
    if (!growth_bytes_per_second_.has_value() || *growth_bytes_per_second_ <= 0.0) {
        return {MemoryCurrentDurationAvailability::non_positive_growth, std::nullopt};
    }

    const long double scaled_variance = 1000.0L * time_variance_sum_;
    const long double milliseconds_per_byte = scaled_variance / time_memory_covariance_sum_;
    const long double remainder =
        std::fma(-milliseconds_per_byte, time_memory_covariance_sum_, scaled_variance) +
        std::fma(1000.0L, time_variance_sum_, -scaled_variance);
    const auto bytes_remaining = target_memory_bytes - current_memory_bytes;
    const long double correction =
        static_cast<long double>(bytes_remaining) * remainder / time_memory_covariance_sum_;
    const long double duration_ms = std::floor(
        std::fma(static_cast<long double>(bytes_remaining), milliseconds_per_byte, correction));
    const long double upper_bound = std::ldexp(1.0L, std::numeric_limits<std::uint64_t>::digits);
    if (!std::isfinite(duration_ms) || duration_ms < 0.0L || duration_ms >= upper_bound) {
        return {MemoryCurrentDurationAvailability::duration_out_of_range, std::nullopt};
    }

    return {MemoryCurrentDurationAvailability::available, static_cast<std::uint64_t>(duration_ms)};
}

void MemoryCurrentForecastAccumulator::add(const MemoryCurrentObservation &observation) {
    if (!first_captured_at_.has_value()) {
        first_captured_at_ = observation.captured_at;
        first_current_memory_bytes_ = observation.current_memory_bytes;
    } else if (observation.captured_at <= *last_captured_at_) {
        invalid_series_reason_ = MemoryCurrentForecastAvailability::non_increasing_timestamps;
    }

    const long double time_seconds =
        (static_cast<long double>(observation.captured_at.time_since_epoch().count()) -
         static_cast<long double>(first_captured_at_->time_since_epoch().count())) /
        static_cast<long double>(1000U);
    const long double memory_bytes =
        observation.current_memory_bytes >= *first_current_memory_bytes_
            ? static_cast<long double>(observation.current_memory_bytes -
                                       *first_current_memory_bytes_)
            : -static_cast<long double>(*first_current_memory_bytes_ -
                                        observation.current_memory_bytes);
    ++observation_count_;
    const long double count = static_cast<long double>(observation_count_);
    const long double time_delta = time_seconds - mean_time_seconds_;
    const long double memory_delta = memory_bytes - mean_memory_bytes_;
    mean_time_seconds_ += time_delta / count;
    mean_memory_bytes_ += memory_delta / count;
    const long double weight = static_cast<long double>(observation_count_ - 1U) / count;
    time_variance_sum_ += weight * time_delta * time_delta;
    time_memory_covariance_sum_ += weight * time_delta * memory_delta;
    last_captured_at_ = observation.captured_at;
}

MemoryCurrentForecast MemoryCurrentForecastAccumulator::finish() const {
    const auto unavailable = [](const MemoryCurrentForecastAvailability availability) {
        return MemoryCurrentForecast{availability, std::nullopt, 0.0L, 0.0L, 0.0L, 0.0L, 0.0L};
    };

    if (observation_count_ < kMinimumObservationCount) {
        return unavailable(MemoryCurrentForecastAvailability::insufficient_observations);
    }
    if (invalid_series_reason_.has_value()) {
        return unavailable(*invalid_series_reason_);
    }

    const long double observation_window_ms =
        static_cast<long double>(last_captured_at_->time_since_epoch().count()) -
        static_cast<long double>(first_captured_at_->time_since_epoch().count());
    if (observation_window_ms <
        static_cast<long double>(
            std::chrono::duration_cast<std::chrono::milliseconds>(kMinimumObservationWindow)
                .count())) {
        return unavailable(MemoryCurrentForecastAvailability::insufficient_observation_window);
    }

    const long double slope = time_memory_covariance_sum_ / time_variance_sum_;
    const long double intercept_offset = mean_memory_bytes_ - slope * mean_time_seconds_;
    const double growth = static_cast<double>(slope);
    const long double latest_offset_seconds =
        (static_cast<long double>(last_captured_at_->time_since_epoch().count()) -
         static_cast<long double>(first_captured_at_->time_since_epoch().count())) /
        static_cast<long double>(1000U);
    if (!std::isfinite(slope) || !std::isfinite(intercept_offset) || !std::isfinite(growth) ||
        !std::isfinite(latest_offset_seconds)) {
        return unavailable(MemoryCurrentForecastAvailability::numeric_result_unavailable);
    }

    return {MemoryCurrentForecastAvailability::available,
            growth,
            static_cast<long double>(*first_current_memory_bytes_),
            intercept_offset,
            latest_offset_seconds,
            time_variance_sum_,
            time_memory_covariance_sum_};
}

MemoryCurrentForecast
fit_memory_current_forecast(const std::span<const MemoryCurrentObservation> observations) {
    MemoryCurrentForecastAccumulator accumulator;
    for (const MemoryCurrentObservation &observation : observations) {
        accumulator.add(observation);
    }
    return accumulator.finish();
}

} // namespace memoptima::forecast
