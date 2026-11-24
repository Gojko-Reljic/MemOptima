#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace memoptima::evidence {

struct ArtifactSessionStart {
    std::uint64_t line_number;
    std::chrono::sys_time<std::chrono::milliseconds> started_at;
    std::uint32_t requested_sample_count;
    std::uint64_t requested_interval_ms;
};

enum class ArtifactMemoryMaximumKind {
    finite,
    unlimited,
    unavailable,
};

struct ArtifactMemoryMaximum {
    ArtifactMemoryMaximumKind kind;
    std::uint64_t bytes{};
};

struct ArtifactContainerObservation {
    std::string container_id;
    std::string cgroup_path;
    std::optional<std::uint64_t> current_memory_bytes;
    ArtifactMemoryMaximum maximum_memory;
};

struct ArtifactSample {
    std::uint64_t line_number;
    std::chrono::sys_time<std::chrono::milliseconds> captured_at;
    std::vector<ArtifactContainerObservation> containers;
};

enum class ArtifactSessionOutcome {
    completed,
    interrupted,
    failed,
};

struct ArtifactSessionEnd {
    std::uint64_t line_number;
    ArtifactSessionOutcome outcome;
    std::chrono::sys_time<std::chrono::milliseconds> ended_at;
};

struct ArtifactTraversalResult {
    bool incomplete;
};

struct ArtifactTraversalError {
    std::string detail;
};

class ArtifactSink {
  public:
    virtual ~ArtifactSink() = default;

    virtual std::expected<void, ArtifactTraversalError>
    consume(const ArtifactSessionStart &start) = 0;
    virtual std::expected<void, ArtifactTraversalError> consume(const ArtifactSample &sample) = 0;
    virtual std::expected<void, ArtifactTraversalError> consume(const ArtifactSessionEnd &end) = 0;
};

// Streams one schema-v1 artifact through the shared validator without following symbolic links
// below root. Callers must supply the artifact path relative to that trusted directory.
[[nodiscard]] std::expected<ArtifactTraversalResult, ArtifactTraversalError>
traverse_artifact_beneath(const std::filesystem::path &root,
                          const std::filesystem::path &relative_path, ArtifactSink &sink);

} // namespace memoptima::evidence
