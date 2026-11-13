#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace memoptima {

struct EvidenceInspectionContainerSummary {
    std::string container_id;
    std::uint32_t samples_seen;
    std::string highest_risk;
    std::uint32_t partial_coverage_samples;
    std::string last_risk;
    std::string last_coverage;
};

struct EvidenceInspectionSummary {
    std::filesystem::path artifact_path;
    std::string application_version;
    std::uint32_t requested_sample_count;
    std::uint32_t completed_sample_count;
    std::uint64_t discovery_warning_count;
    std::uint64_t skipped_container_count;
    bool incomplete;
    std::string terminal_outcome;
    std::vector<EvidenceInspectionContainerSummary> containers;
};

struct EvidenceInspectionError {
    std::string detail;
};

[[nodiscard]] std::expected<EvidenceInspectionSummary, EvidenceInspectionError>
inspect_evidence_artifact(const std::filesystem::path &path);

} // namespace memoptima
