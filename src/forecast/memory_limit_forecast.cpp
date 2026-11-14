#include <memoptima/forecast/memory_limit_forecast.hpp>

#include <algorithm>
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

MemoryLimitForecast forecast_memory_limit(std::span<const MemoryLimitObservation> observations) {
    if (observations.size() < kMinimumObservationCount) {
        return unavailable(MemoryLimitForecastAvailability::insufficient_observations);
    }

    const std::uint64_t memory_max = observations.front().memory_max_bytes;
    for (std::size_t index = 1; index < observations.size(); ++index) {
        if (observations[index].captured_at <= observations[index - 1].captured_at) {
            return unavailable(MemoryLimitForecastAvailability::non_increasing_timestamps);
        }
        if (observations[index].memory_max_bytes != memory_max) {
            return unavailable(MemoryLimitForecastAvailability::unstable_memory_max);
        }
    }

    const auto observation_window =
        observations.back().captured_at - observations.front().captured_at;
    if (observation_window < kMinimumObservationWindow) {
        return unavailable(MemoryLimitForecastAvailability::insufficient_observation_window);
    }

    const MemoryLimitObservation &latest = observations.back();
    if (latest.current_memory_bytes >= memory_max) {
        return unavailable(MemoryLimitForecastAvailability::already_at_or_above_memory_max);
    }

    const auto start = observations.front().captured_at;
    long double mean_time_seconds{};
    long double mean_memory_bytes{};
    for (const MemoryLimitObservation &observation : observations) {
        const auto offset = observation.captured_at - start;
        mean_time_seconds += static_cast<long double>(offset.count()) /
                             std::chrono::milliseconds{1}.count() / 1000.0L;
        mean_memory_bytes += static_cast<long double>(observation.current_memory_bytes);
    }
    mean_time_seconds /= static_cast<long double>(observations.size());
    mean_memory_bytes /= static_cast<long double>(observations.size());

    long double covariance{};
    long double time_variance{};
    for (const MemoryLimitObservation &observation : observations) {
        const auto offset = observation.captured_at - start;
        const long double time_seconds = static_cast<long double>(offset.count()) /
                                         std::chrono::milliseconds{1}.count() / 1000.0L;
        const long double centered_time = time_seconds - mean_time_seconds;
        covariance += centered_time * (static_cast<long double>(observation.current_memory_bytes) -
                                       mean_memory_bytes);
        time_variance += centered_time * centered_time;
    }

    const double growth = static_cast<double>(covariance / time_variance);
    if (!std::isfinite(growth) || growth <= 0.0) {
        return unavailable(MemoryLimitForecastAvailability::non_positive_growth, growth);
    }

    const long double bytes_remaining =
        static_cast<long double>(memory_max - latest.current_memory_bytes);
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

} // namespace memoptima::forecast
