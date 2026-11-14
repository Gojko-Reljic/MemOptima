#include <memoptima/forecast/memory_limit_forecast.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>

namespace memoptima::forecast {
namespace {

MemoryLimitObservation observation(std::int64_t seconds, std::uint64_t current,
                                   std::uint64_t maximum = 3'000) {
    return {
        .captured_at =
            std::chrono::sys_time<std::chrono::milliseconds>{std::chrono::seconds{seconds}},
        .current_memory_bytes = current,
        .memory_max_bytes = maximum,
    };
}

TEST(MemoryLimitForecast, EstimatesTimeFromTheLatestObservedMemoryAndRegressionGrowth) {
    const std::array observations{observation(0, 1'000), observation(10, 1'500),
                                  observation(20, 2'000)};

    const MemoryLimitForecast forecast = forecast_memory_limit(observations);

    ASSERT_EQ(forecast.availability, MemoryLimitForecastAvailability::available);
    ASSERT_TRUE(forecast.growth_bytes_per_second.has_value());
    ASSERT_TRUE(forecast.estimated_time_to_memory_max.has_value());
    EXPECT_DOUBLE_EQ(*forecast.growth_bytes_per_second, 50.0);
    EXPECT_EQ(*forecast.estimated_time_to_memory_max, std::chrono::seconds{20});
}

TEST(MemoryLimitForecast, RejectsInsufficientObservationsAndShortWindows) {
    const std::array too_few{observation(0, 1'000), observation(10, 1'500)};
    const std::array too_short{observation(0, 1'000), observation(1, 1'500), observation(2, 2'000)};

    EXPECT_EQ(forecast_memory_limit(too_few).availability,
              MemoryLimitForecastAvailability::insufficient_observations);
    EXPECT_EQ(forecast_memory_limit(too_short).availability,
              MemoryLimitForecastAvailability::insufficient_observation_window);
}

TEST(MemoryLimitForecast, RejectsUnstableOrInvalidTimeSeries) {
    const std::array unstable_limit{observation(0, 1'000), observation(10, 1'500, 4'000),
                                    observation(20, 2'000)};
    const std::array unordered_time{observation(0, 1'000), observation(20, 1'500),
                                    observation(10, 2'000)};

    EXPECT_EQ(forecast_memory_limit(unstable_limit).availability,
              MemoryLimitForecastAvailability::unstable_memory_max);
    EXPECT_EQ(forecast_memory_limit(unordered_time).availability,
              MemoryLimitForecastAvailability::non_increasing_timestamps);
}

TEST(MemoryLimitForecast, RefusesAForecastForBoundaryOrNonGrowingMemory) {
    const std::array at_limit{observation(0, 1'000), observation(10, 2'000),
                              observation(20, 3'000)};
    const std::array falling{observation(0, 2'000), observation(10, 1'500), observation(20, 1'000)};

    EXPECT_EQ(forecast_memory_limit(at_limit).availability,
              MemoryLimitForecastAvailability::already_at_or_above_memory_max);
    const MemoryLimitForecast falling_forecast = forecast_memory_limit(falling);
    EXPECT_EQ(falling_forecast.availability, MemoryLimitForecastAvailability::non_positive_growth);
    ASSERT_TRUE(falling_forecast.growth_bytes_per_second.has_value());
    EXPECT_DOUBLE_EQ(*falling_forecast.growth_bytes_per_second, -50.0);
}

TEST(MemoryLimitForecast, AccumulatesAStreamingSeriesWithoutRetainingObservations) {
    MemoryLimitForecastAccumulator accumulator;
    accumulator.add(observation(0, 1'000));
    accumulator.add(observation(10, 1'500));
    accumulator.add(observation(20, 2'000));

    const MemoryLimitForecast forecast = accumulator.finish();

    ASSERT_EQ(forecast.availability, MemoryLimitForecastAvailability::available);
    ASSERT_TRUE(forecast.estimated_time_to_memory_max.has_value());
    EXPECT_EQ(*forecast.estimated_time_to_memory_max, std::chrono::seconds{20});
}

} // namespace
} // namespace memoptima::forecast
