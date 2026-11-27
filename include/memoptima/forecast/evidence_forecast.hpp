#pragma once

#include <memoptima/forecast/memory_limit_forecast.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace memoptima::forecast {

enum class EvidenceForecastStatus {
    available,
    not_applicable_unlimited_memory_max,
    unavailable_container_not_observed_continuously,
    unavailable_current_memory,
    unavailable_memory_max,
    unavailable_invalid_timestamp,
    unavailable_insufficient_observations,
    unavailable_insufficient_observation_window,
    unavailable_non_increasing_timestamps,
    unavailable_unstable_memory_max,
    unavailable_already_at_or_above_memory_max,
    unavailable_non_positive_growth,
    unavailable_time_to_limit_out_of_range,
    unavailable_numeric_result,
};

struct ContainerEvidenceForecast {
    std::string container_id;
    std::uint32_t observed_sample_count;
    EvidenceForecastStatus status;
    std::optional<double> growth_bytes_per_second;
    std::optional<std::chrono::milliseconds> estimated_time_to_memory_max;
};

struct EvidenceForecastSummary {
    std::filesystem::path artifact_path;
    bool incomplete;
    std::uint32_t sample_count;
    std::vector<ContainerEvidenceForecast> containers;
};

struct EvidenceForecastError {
    std::string detail;
};

[[nodiscard]] std::expected<EvidenceForecastSummary, EvidenceForecastError>
forecast_evidence_artifact(const std::filesystem::path &path);

} // namespace memoptima::forecast
