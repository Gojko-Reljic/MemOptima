#pragma once

#include <memoptima/docker_container_memory_snapshot.hpp>
#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

namespace memoptima::sampling {

struct SamplingSessionConfiguration {
    SamplingCount count;
    SamplingInterval interval;
    std::filesystem::path output_path;
};

enum class SamplingSessionRunErrorCode {
    preflight_failed,
    output_creation_failed,
    serialization_failed,
    output_write_failed,
    output_flush_failed,
};

struct SamplingSessionRunError {
    SamplingSessionRunErrorCode code;
    std::string detail;
};

struct SamplingSessionResult {
    SamplingSessionOutcome outcome;
    std::uint32_t completed_sample_count;
    std::optional<SamplingSessionFailure> failure;
};

// Dependencies isolate time, interruption, preflight, and container discovery for deterministic
// tests.
struct SamplingSessionDependencies {
    std::function<std::expected<void, std::string>()> preflight;
    std::function<DockerContainerScanResult()> scan_containers;
    std::function<std::chrono::system_clock::time_point()> current_utc_time;
    std::function<std::chrono::steady_clock::time_point()> current_steady_time;
    std::function<void(std::chrono::milliseconds)> sleep_for;
    std::function<bool()> stop_requested;
};

[[nodiscard]] std::expected<SamplingSessionResult, SamplingSessionRunError>
run_sampling_session(const SamplingSessionConfiguration &configuration,
                     const SamplingSessionDependencies &dependencies);

} // namespace memoptima::sampling
