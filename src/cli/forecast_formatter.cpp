#include "forecast_formatter.hpp"

#include "formatting.hpp"

#include <iomanip>
#include <sstream>

namespace memoptima::cli {
namespace {
std::string status_name(forecast::EvidenceForecastStatus status) {
    using enum forecast::EvidenceForecastStatus;
    switch (status) {
    case available:
        return "Available";
    case not_applicable_unlimited_memory_max:
        return "Not applicable: unlimited memory.max";
    case unavailable_container_not_observed_continuously:
        return "Unavailable: not observed continuously";
    case unavailable_current_memory:
        return "Unavailable: memory.current";
    case unavailable_memory_max:
        return "Unavailable: memory.max";
    case unavailable_invalid_timestamp:
        return "Unavailable: invalid timestamp";
    case unavailable_insufficient_observations:
        return "Unavailable: fewer than 3 samples";
    case unavailable_insufficient_observation_window:
        return "Unavailable: under 10 seconds";
    case unavailable_non_increasing_timestamps:
        return "Unavailable: non-increasing timestamps";
    case unavailable_unstable_memory_max:
        return "Unavailable: changing memory.max";
    case unavailable_already_at_or_above_memory_max:
        return "Unavailable: at or above memory.max";
    case unavailable_non_positive_growth:
        return "Unavailable: non-positive growth";
    case unavailable_time_to_limit_out_of_range:
        return "Unavailable: estimate out of range";
    }
    return "Unavailable";
}
} // namespace

std::string format_evidence_forecast(const forecast::EvidenceForecastSummary &summary) {
    std::ostringstream output;
    output << "Evidence forecast\n\nPath: " << escape_terminal_text(summary.artifact_path.string())
           << "\nSamples: " << summary.sample_count
           << "\nArtifact: " << (summary.incomplete ? "Incomplete" : "Closed") << "\n\n"
           << std::left << std::setw(66) << "CONTAINER ID" << std::setw(10) << "SAMPLES"
           << std::setw(22) << "GROWTH" << "STATUS / ESTIMATE\n";
    for (const auto &container : summary.containers) {
        output << std::left << std::setw(66) << escape_terminal_text(container.container_id)
               << std::setw(10) << container.observed_sample_count;
        if (container.status == forecast::EvidenceForecastStatus::available &&
            container.growth_bytes_per_second.has_value()) {
            output << std::setw(22)
                   << (std::to_string(*container.growth_bytes_per_second) + " B/s");
        } else
            output << std::setw(22) << "N/A";
        output << status_name(container.status);
        if (container.status == forecast::EvidenceForecastStatus::available &&
            container.estimated_time_to_memory_max.has_value())
            output << "; estimate: "
                   << format_duration(static_cast<std::uint64_t>(
                                          container.estimated_time_to_memory_max->count()) *
                                      1000)
                   << " to memory.max";
        output << '\n';
    }
    output << "\nForecast estimates a memory.max crossing, not an OOM event or memory leak.\n";
    return output.str();
}
} // namespace memoptima::cli
