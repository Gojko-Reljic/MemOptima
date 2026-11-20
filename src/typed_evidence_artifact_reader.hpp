#pragma once

#include "evidence_artifact_reader.hpp"

#include <memoptima/docker_container_memory_snapshot.hpp>
#include <memoptima/sampling/evidence_models.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <vector>

namespace memoptima::detail {

struct TypedEvidenceSessionStart {
    std::uint64_t line_number;
    std::chrono::sys_time<std::chrono::milliseconds> started_at;
    std::uint32_t requested_sample_count;
    std::uint64_t requested_interval_ms;
};

enum class TypedEvidenceMemoryMaximumKind {
    finite,
    unlimited,
    unavailable,
};

struct TypedEvidenceMemoryMaximum {
    TypedEvidenceMemoryMaximumKind kind;
    std::uint64_t bytes{};
};

struct TypedEvidenceContainerObservation {
    DockerContainerId container_id;
    std::filesystem::path cgroup_path;
    std::optional<std::uint64_t> current_memory_bytes;
    TypedEvidenceMemoryMaximum maximum_memory;
};

struct TypedEvidenceSample {
    std::uint64_t line_number;
    std::chrono::sys_time<std::chrono::milliseconds> captured_at;
    std::vector<TypedEvidenceContainerObservation> containers;
};

struct TypedEvidenceSessionEnd {
    std::uint64_t line_number;
    sampling::SamplingSessionOutcome outcome;
    std::chrono::sys_time<std::chrono::milliseconds> ended_at;
};

class TypedEvidenceArtifactSink {
  public:
    virtual ~TypedEvidenceArtifactSink() = default;

    virtual std::expected<void, EvidenceArtifactTraversalError>
    consume(const TypedEvidenceSessionStart &start) = 0;
    virtual std::expected<void, EvidenceArtifactTraversalError>
    consume(const TypedEvidenceSample &sample) = 0;
    virtual std::expected<void, EvidenceArtifactTraversalError>
    consume(const TypedEvidenceSessionEnd &end) = 0;
};

// Streams schema-v1 evidence through the existing validator and exposes only the typed v0.7
// projection needed by cross-session trends.
[[nodiscard]] std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_typed_evidence_artifact(const std::filesystem::path &path,
                                 TypedEvidenceArtifactSink &sink);

[[nodiscard]] std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_typed_evidence_artifact_beneath(const std::filesystem::path &root,
                                         const std::filesystem::path &relative_path,
                                         TypedEvidenceArtifactSink &sink);

} // namespace memoptima::detail
