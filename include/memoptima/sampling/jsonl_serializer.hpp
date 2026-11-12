#pragma once

#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/output_contract.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace memoptima::sampling {

enum class SamplingJsonlSerializationErrorCode {
    invalid_session_end,
    timestamp_format_failed,
    json_serialization_failed,
};

struct SamplingJsonlSerializationError {
    SamplingJsonlSerializationErrorCode code;
    std::string detail;
};

// Each function returns one complete JSON object without its trailing JSON Lines newline.
[[nodiscard]] std::expected<std::string, SamplingJsonlSerializationError>
serialize_session_start(SamplingCount requested_count, SamplingInterval requested_interval,
                        std::chrono::system_clock::time_point started_at);

[[nodiscard]] std::expected<std::string, SamplingJsonlSerializationError>
serialize_sample(const SamplingEvidenceSample &sample);

[[nodiscard]] std::expected<std::string, SamplingJsonlSerializationError>
serialize_session_end(SamplingSessionOutcome outcome, std::uint32_t completed_sample_count,
                      std::chrono::system_clock::time_point ended_at,
                      const std::optional<SamplingSessionFailure> &failure);

} // namespace memoptima::sampling
