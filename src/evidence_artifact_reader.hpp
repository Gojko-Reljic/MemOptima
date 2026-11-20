#pragma once

#include <memoptima/evidence_record_parser.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>

namespace memoptima::detail {

struct EvidenceArtifactRecord {
    std::uint64_t line_number;
    EvidenceRecordType type;
    std::string canonical_json;
};

struct EvidenceArtifactTraversalResult {
    bool incomplete;
};

struct EvidenceArtifactTraversalError {
    std::string detail;
};

class EvidenceArtifactRecordSink {
  public:
    virtual ~EvidenceArtifactRecordSink() = default;

    virtual std::expected<void, EvidenceArtifactTraversalError>
    consume(const EvidenceArtifactRecord &record) = 0;
};

// Streams a strictly validated artifact and shares its lifecycle rules between independent
// consumers.
[[nodiscard]] std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_evidence_artifact(const std::filesystem::path &path, EvidenceArtifactRecordSink &sink);

// Streams an artifact selected by a manifest without following symbolic links below its root.
[[nodiscard]] std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_evidence_artifact_beneath(const std::filesystem::path &root,
                                   const std::filesystem::path &relative_path,
                                   EvidenceArtifactRecordSink &sink);

} // namespace memoptima::detail
