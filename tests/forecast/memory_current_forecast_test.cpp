#include <memoptima/forecast/memory_current_forecast.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>

namespace memoptima::forecast {
namespace {

MemoryCurrentObservation observation(const std::int64_t seconds, const std::uint64_t current) {
    return {
        .captured_at = MemoryCurrentForecastTimePoint{std::chrono::seconds{seconds}},
        .current_memory_bytes = current,
    };
}

TEST(MemoryCurrentForecast, FitsGrowthAndProjectsForwardFromTheLatestObservation) {
    const std::array observations{observation(0, 1'000U), observation(10, 1'500U),
                                  observation(20, 2'000U)};

    const auto forecast = fit_memory_current_forecast(observations);

    ASSERT_EQ(forecast.availability(), MemoryCurrentForecastAvailability::available);
    ASSERT_TRUE(forecast.growth_bytes_per_second().has_value());
    EXPECT_DOUBLE_EQ(*forecast.growth_bytes_per_second(), 50.0);

    const auto projection = forecast.project_after(std::chrono::seconds{5});
    ASSERT_EQ(projection.availability, MemoryCurrentProjectionAvailability::available);
    ASSERT_TRUE(projection.current_memory_bytes.has_value());
    EXPECT_EQ(*projection.current_memory_bytes, 2'250U);
}

TEST(MemoryCurrentForecast, RequiresSufficientOrderedHistory) {
    const std::array too_few{observation(0, 1'000U), observation(10, 1'500U)};
    const std::array too_short{observation(0, 1'000U), observation(1, 1'500U),
                               observation(2, 2'000U)};
    const std::array unordered{observation(0, 1'000U), observation(20, 1'500U),
                               observation(10, 2'000U)};

    EXPECT_EQ(fit_memory_current_forecast(too_few).availability(),
              MemoryCurrentForecastAvailability::insufficient_observations);
    EXPECT_EQ(fit_memory_current_forecast(too_short).availability(),
              MemoryCurrentForecastAvailability::insufficient_observation_window);
    EXPECT_EQ(fit_memory_current_forecast(unordered).availability(),
              MemoryCurrentForecastAvailability::non_increasing_timestamps);
}

TEST(MemoryCurrentForecast, ClampsNegativeProjectionAndRoundsToBytes) {
    const std::array falling{observation(0, 200U), observation(10, 100U), observation(20, 0U)};

    const auto forecast = fit_memory_current_forecast(falling);
    const auto projection = forecast.project_after(std::chrono::seconds{5});

    ASSERT_EQ(projection.availability, MemoryCurrentProjectionAvailability::available);
    ASSERT_TRUE(projection.current_memory_bytes.has_value());
    EXPECT_EQ(*projection.current_memory_bytes, 0U);
}

TEST(MemoryCurrentForecast, RoundsARepresentableFractionalProjectionToTheNearestByte) {
    const std::array observations{observation(0, 0U), observation(10, 1U), observation(20, 1U)};

    const auto projection =
        fit_memory_current_forecast(observations).project_after(std::chrono::seconds{10});

    ASSERT_EQ(projection.availability, MemoryCurrentProjectionAvailability::available);
    ASSERT_TRUE(projection.current_memory_bytes.has_value());
    EXPECT_EQ(*projection.current_memory_bytes, 2U);
}

TEST(MemoryCurrentForecast, RejectsNegativeProjectionDurations) {
    const std::array observations{observation(0, 1'000U), observation(10, 1'500U),
                                  observation(20, 2'000U)};

    const auto projection =
        fit_memory_current_forecast(observations).project_after(-std::chrono::milliseconds{1});

    EXPECT_EQ(projection.availability,
              MemoryCurrentProjectionAvailability::numeric_result_unavailable);
    EXPECT_FALSE(projection.current_memory_bytes.has_value());
}

TEST(MemoryCurrentForecast, GivesTimestampOrderingPrecedenceOverAnInvalidWindow) {
    const std::array unordered{observation(0, 1'000U), observation(20, 1'500U),
                               observation(-1, 2'000U)};

    EXPECT_EQ(fit_memory_current_forecast(unordered).availability(),
              MemoryCurrentForecastAvailability::non_increasing_timestamps);
}

} // namespace
} // namespace memoptima::forecast
