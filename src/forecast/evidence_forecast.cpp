#include <memoptima/forecast/evidence_forecast.hpp>

#include "evidence_artifact_reader.hpp"
#include "utc_timestamp_parser.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <map>

namespace memoptima::forecast {
namespace {
using Json = nlohmann::ordered_json;

struct State {
    MemoryLimitForecastAccumulator accumulator;
    std::uint32_t observed{};
    EvidenceForecastStatus status{EvidenceForecastStatus::available};
};

bool parse_decimal(const Json &value, std::uint64_t &parsed) {
    if (!value.is_string())
        return false;
    const auto text = value.get<std::string>();
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    return error == std::errc{} && end == text.data() + text.size();
}

EvidenceForecastStatus map_status(MemoryLimitForecastAvailability value) {
    switch (value) {
    case MemoryLimitForecastAvailability::available:
        return EvidenceForecastStatus::available;
    case MemoryLimitForecastAvailability::insufficient_observations:
        return EvidenceForecastStatus::unavailable_insufficient_observations;
    case MemoryLimitForecastAvailability::insufficient_observation_window:
        return EvidenceForecastStatus::unavailable_insufficient_observation_window;
    case MemoryLimitForecastAvailability::non_increasing_timestamps:
        return EvidenceForecastStatus::unavailable_non_increasing_timestamps;
    case MemoryLimitForecastAvailability::unstable_memory_max:
        return EvidenceForecastStatus::unavailable_unstable_memory_max;
    case MemoryLimitForecastAvailability::already_at_or_above_memory_max:
        return EvidenceForecastStatus::unavailable_already_at_or_above_memory_max;
    case MemoryLimitForecastAvailability::non_positive_growth:
        return EvidenceForecastStatus::unavailable_non_positive_growth;
    case MemoryLimitForecastAvailability::time_to_limit_out_of_range:
        return EvidenceForecastStatus::unavailable_time_to_limit_out_of_range;
    }
    return EvidenceForecastStatus::unavailable_non_positive_growth;
}

class Sink final : public detail::EvidenceArtifactRecordSink {
  public:
    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::EvidenceArtifactRecord &record) override {
        if (record.type != EvidenceRecordType::sample)
            return {};
        ++samples_;
        const Json value = Json::parse(record.canonical_json);
        const auto timestamp =
            detail::parse_canonical_utc_timestamp(value.at("captured_at_utc").get<std::string>());
        for (const auto &container : value.at("containers")) {
            auto &state = states_[container.at("container_id").get<std::string>()];
            ++state.observed;
            if (state.status != EvidenceForecastStatus::available) {
                continue;
            }
            if (!timestamp) {
                state.status = EvidenceForecastStatus::unavailable_invalid_timestamp;
                continue;
            }
            const auto &memory = container.at("memory");
            if (memory.at("current_bytes").is_null()) {
                state.status = EvidenceForecastStatus::unavailable_current_memory;
                continue;
            }
            if (memory.at("max_bytes").is_null()) {
                state.status = EvidenceForecastStatus::unavailable_memory_max;
                continue;
            }
            if (memory.at("max_bytes") == "max") {
                state.status = EvidenceForecastStatus::not_applicable_unlimited_memory_max;
                continue;
            }
            std::uint64_t current{};
            std::uint64_t maximum{};
            if (!parse_decimal(memory.at("current_bytes"), current) ||
                !parse_decimal(memory.at("max_bytes"), maximum)) {
                state.status = EvidenceForecastStatus::unavailable_current_memory;
                continue;
            }
            state.accumulator.add({*timestamp, current, maximum});
        }
        return {};
    }
    std::uint32_t samples() const { return samples_; }
    std::map<std::string, State> &states() { return states_; }

  private:
    std::uint32_t samples_{};
    std::map<std::string, State> states_;
};
} // namespace
std::expected<EvidenceForecastSummary, EvidenceForecastError>
forecast_evidence_artifact(const std::filesystem::path &path) {
    Sink sink;
    const auto traversal = detail::traverse_evidence_artifact(path, sink);
    if (!traversal)
        return std::unexpected(EvidenceForecastError{traversal.error().detail});
    EvidenceForecastSummary result{path, traversal->incomplete, sink.samples(), {}};
    for (auto &[id, state] : sink.states()) {
        if (state.observed != sink.samples())
            state.status = EvidenceForecastStatus::unavailable_container_not_observed_continuously;
        const auto forecast = state.accumulator.finish();
        if (state.status == EvidenceForecastStatus::available)
            state.status = map_status(forecast.availability);
        result.containers.push_back({id, state.observed, state.status,
                                     forecast.growth_bytes_per_second,
                                     forecast.estimated_time_to_memory_max});
    }
    return result;
}
} // namespace memoptima::forecast
