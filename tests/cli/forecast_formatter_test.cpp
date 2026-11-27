#include "cli/forecast_formatter.hpp"

#include <gtest/gtest.h>

#include <chrono>

namespace memoptima::cli {
namespace {

TEST(ForecastFormatter, FormatsAvailableAndUnavailableContainerForecasts) {
    const forecast::EvidenceForecastSummary summary{
        .artifact_path = "/tmp/evidence.jsonl",
        .incomplete = false,
        .sample_count = 3,
        .containers = {
            {.container_id = "aaaaaaaa",
             .observed_sample_count = 3,
             .status = forecast::EvidenceForecastStatus::available,
             .growth_bytes_per_second = 42.5,
             .estimated_time_to_memory_max = std::chrono::seconds{10}},
            {.container_id = "bbbbbbbb",
             .observed_sample_count = 3,
             .status = forecast::EvidenceForecastStatus::not_applicable_unlimited_memory_max,
             .growth_bytes_per_second = std::nullopt,
             .estimated_time_to_memory_max = std::nullopt}}};

    const std::string output = format_evidence_forecast(summary);

    EXPECT_NE(output.find("Evidence forecast"), std::string::npos);
    EXPECT_NE(output.find("CONTAINER ID"), std::string::npos);
    EXPECT_NE(output.find("aaaaaaaa"), std::string::npos);
    EXPECT_NE(output.find("42.500000 B/s"), std::string::npos);
    EXPECT_NE(output.find("estimate: 10.00 s to memory.max"), std::string::npos);
    EXPECT_NE(output.find("Not applicable: unlimited memory.max"), std::string::npos);
    EXPECT_NE(output.find("not an OOM event or memory leak"), std::string::npos);
}

TEST(ForecastFormatter, MarksIncompleteArtifactsWithoutChangingContainerStatus) {
    const forecast::EvidenceForecastSummary summary{
        .artifact_path = "/tmp/incomplete.jsonl",
        .incomplete = true,
        .sample_count = 2,
        .containers = {
            {.container_id = "cccccccc",
             .observed_sample_count = 2,
             .status = forecast::EvidenceForecastStatus::unavailable_insufficient_observations,
             .growth_bytes_per_second = std::nullopt,
             .estimated_time_to_memory_max = std::nullopt}}};

    const std::string output = format_evidence_forecast(summary);

    EXPECT_NE(output.find("Artifact: Incomplete"), std::string::npos);
    EXPECT_NE(output.find("Unavailable: fewer than 3 samples"), std::string::npos);
}

TEST(ForecastFormatter, OmitsNumbersForAnUnavailableResult) {
    const forecast::EvidenceForecastSummary summary{
        .artifact_path = "/tmp/unavailable.jsonl",
        .incomplete = false,
        .sample_count = 4,
        .containers = {{.container_id = "dddddddd",
                        .observed_sample_count = 4,
                        .status = forecast::EvidenceForecastStatus::unavailable_current_memory,
                        .growth_bytes_per_second = 10.0,
                        .estimated_time_to_memory_max = std::chrono::seconds{10}}}};

    const std::string output = format_evidence_forecast(summary);

    EXPECT_NE(output.find("Unavailable: memory.current"), std::string::npos);
    EXPECT_EQ(output.find("10.000000 B/s"), std::string::npos);
    EXPECT_EQ(output.find("estimate:"), std::string::npos);
}

TEST(ForecastFormatter, ExplainsAnUnavailableNumericResult) {
    const forecast::EvidenceForecastSummary summary{
        .artifact_path = "/tmp/numeric.jsonl",
        .incomplete = false,
        .sample_count = 3,
        .containers = {{.container_id = "eeeeeeee",
                        .observed_sample_count = 3,
                        .status = forecast::EvidenceForecastStatus::unavailable_numeric_result,
                        .growth_bytes_per_second = std::nullopt,
                        .estimated_time_to_memory_max = std::nullopt}}};

    EXPECT_NE(format_evidence_forecast(summary).find("Unavailable: numeric result"),
              std::string::npos);
}

} // namespace
} // namespace memoptima::cli
