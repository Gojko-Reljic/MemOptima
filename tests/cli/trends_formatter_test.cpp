#include "cli/trends_formatter.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace memoptima::cli {
namespace {

trends::TrendEpochResult make_epoch(std::string container_id, std::string cgroup_path) {
    return {
        .identity = {.container_id = std::move(container_id),
                     .cgroup_path = std::move(cgroup_path)},
        .epoch_index = 1U,
        .first_observed_at = trends::TrendTimePoint{},
        .last_observed_at = trends::TrendTimePoint{},
        .observation_count = 12U,
        .observation_window_ms = 60'000U,
        .largest_gap_ms = 5'000U,
        .quality = trends::TrendEvidenceQuality::continuous,
        .contains_incomplete_session = false,
        .termination_reason = std::nullopt,
        .current_memory = {.first_bytes = 100U,
                           .last_bytes = 200U,
                           .rate_status = trends::TrendRateStatus::available,
                           .growth_bytes_per_second = 1'310'720.0},
        .maximum_memory = {.kind = trends::TrendMemoryMaximumKind::finite, .bytes = 10'000'000U},
        .time_to_limit = {.status = trends::TrendTimeToLimitStatus::available,
                          .estimated_duration_ms = 860'000U}};
}

trends::TrendAnalysis make_analysis(std::vector<trends::TrendEpochResult> epochs) {
    return {.summary = {}, .epochs = std::move(epochs)};
}

std::vector<std::string> lines(std::string_view value) {
    std::vector<std::string> result;
    std::size_t position{};
    while (position < value.size()) {
        const auto end = value.find('\n', position);
        if (end == std::string_view::npos)
            break;
        result.emplace_back(value.substr(position, end - position));
        position = end + 1U;
    }
    return result;
}

TEST(TrendsFormatter, PrintsExactEmptyAnalysisMessage) {
    EXPECT_EQ(format_trend_analysis(make_analysis({})),
              "No container observation epochs found in manifest.\n");
}

TEST(TrendsFormatter, FormatsAllColumnsWithoutTrailingWhitespace) {
    const auto output = format_trend_analysis(
        make_analysis({make_epoch("a1b2c3d4e5f6deadbeef", "/sys/fs/cgroup/docker/a1b2c3d4e5f6")}));

    EXPECT_NE(output.find("CONTAINER ID"), std::string::npos);
    EXPECT_NE(output.find("CGROUP PATH"), std::string::npos);
    EXPECT_NE(output.find("EPOCH"), std::string::npos);
    EXPECT_NE(output.find("SAMPLES"), std::string::npos);
    EXPECT_NE(output.find("WINDOW"), std::string::npos);
    EXPECT_NE(output.find("QUALITY"), std::string::npos);
    EXPECT_NE(output.find("RATE"), std::string::npos);
    EXPECT_NE(output.find("TIME TO LIMIT"), std::string::npos);
    EXPECT_NE(output.find("STATUS"), std::string::npos);
    EXPECT_NE(output.find("a1b2c3d4e5f6"), std::string::npos);
    EXPECT_NE(output.find("1m 00s"), std::string::npos);
    EXPECT_NE(output.find("continuous"), std::string::npos);
    EXPECT_NE(output.find("+1.25 MiB/s"), std::string::npos);
    EXPECT_NE(output.find("14m 20s"), std::string::npos);
    EXPECT_NE(output.find("available"), std::string::npos);

    for (const auto &line : lines(output))
        EXPECT_FALSE(line.ends_with(' '));
}

TEST(TrendsFormatter, PreservesEpochOrderAndExpandsDockerPrefixesForDistinctIds) {
    auto first = make_epoch("aaaaaaaaaaaa111111111111", "/first");
    first.epoch_index = 3U;
    auto second = make_epoch("aaaaaaaaaaaa222222222222", "/second");
    second.epoch_index = 1U;

    const auto output = format_trend_analysis(make_analysis({first, second}));

    EXPECT_NE(output.find("aaaaaaaaaaaa1"), std::string::npos);
    EXPECT_NE(output.find("aaaaaaaaaaaa2"), std::string::npos);
    EXPECT_LT(output.find("/first"), output.find("/second"));
}

TEST(TrendsFormatter, FormatsRateUnitsAndNormalizesNegativeZero) {
    auto epoch = make_epoch("rate-container", "/rate");
    epoch.current_memory.growth_bytes_per_second = -0.0001;

    const auto output = format_trend_analysis(make_analysis({epoch}));

    EXPECT_NE(output.find("+0.00 B/s"), std::string::npos);
    EXPECT_EQ(output.find("-0.00 B/s"), std::string::npos);
}

TEST(TrendsFormatter, DoesNotNarrowAnUnrepresentableObservationWindow) {
    auto epoch = make_epoch("large-window", "/large-window");
    epoch.observation_window_ms = std::numeric_limits<std::uint64_t>::max();
    epoch.time_to_limit.estimated_duration_ms = std::nullopt;

    const auto output = format_trend_analysis(make_analysis({epoch}));
    const auto output_lines = lines(output);

    ASSERT_EQ(output_lines.size(), 2U);
    const auto window_column = output_lines.front().find("WINDOW");
    ASSERT_NE(window_column, std::string::npos);
    EXPECT_EQ(output_lines[1].substr(window_column, std::string{"WINDOW"}.size()), "   N/A");
}

TEST(TrendsFormatter, FormatsEvidenceQualityAndUnavailableStatuses) {
    struct Case {
        trends::TrendEvidenceQuality quality;
        bool incomplete;
        trends::TrendTimeToLimitStatus status;
        trends::TrendRateStatus rate_status;
        std::string_view expected_quality;
        std::string_view expected_status;
    };
    const std::vector<Case> cases{
        {trends::TrendEvidenceQuality::continuous, false,
         trends::TrendTimeToLimitStatus::rate_unavailable,
         trends::TrendRateStatus::insufficient_observations, "continuous",
         "insufficient observations"},
        {trends::TrendEvidenceQuality::sparse, false,
         trends::TrendTimeToLimitStatus::rate_unavailable,
         trends::TrendRateStatus::insufficient_duration, "sparse",
         "insufficient observation window"},
        {trends::TrendEvidenceQuality::continuous, true,
         trends::TrendTimeToLimitStatus::non_positive_growth, trends::TrendRateStatus::available,
         "continuous (incomplete)", "non-positive growth"},
        {trends::TrendEvidenceQuality::sparse, true,
         trends::TrendTimeToLimitStatus::sparse_evidence, trends::TrendRateStatus::available,
         "sparse (incomplete)", "sparse evidence"},
        {trends::TrendEvidenceQuality::continuous, false,
         trends::TrendTimeToLimitStatus::unlimited_maximum, trends::TrendRateStatus::available,
         "continuous", "unlimited memory.max"},
        {trends::TrendEvidenceQuality::continuous, false,
         trends::TrendTimeToLimitStatus::unavailable_maximum, trends::TrendRateStatus::available,
         "continuous", "unavailable memory.max"},
        {trends::TrendEvidenceQuality::continuous, false,
         trends::TrendTimeToLimitStatus::already_at_or_above_maximum,
         trends::TrendRateStatus::available, "continuous", "at or above memory.max"},
        {trends::TrendEvidenceQuality::continuous, false,
         trends::TrendTimeToLimitStatus::duration_out_of_range, trends::TrendRateStatus::available,
         "continuous", "estimate out of range"}};

    for (const auto &test_case : cases) {
        auto epoch = make_epoch("container-" + std::string{test_case.expected_status}, "/test");
        epoch.quality = test_case.quality;
        epoch.contains_incomplete_session = test_case.incomplete;
        epoch.current_memory.rate_status = test_case.rate_status;
        if (test_case.rate_status != trends::TrendRateStatus::available)
            epoch.current_memory.growth_bytes_per_second = std::nullopt;
        epoch.time_to_limit.status = test_case.status;
        epoch.time_to_limit.estimated_duration_ms = std::nullopt;

        const auto output = format_trend_analysis(make_analysis({std::move(epoch)}));
        EXPECT_NE(output.find(test_case.expected_quality), std::string::npos);
        EXPECT_NE(output.find(test_case.expected_status), std::string::npos);
        EXPECT_NE(output.find("N/A"), std::string::npos);
    }
}

} // namespace
} // namespace memoptima::cli
