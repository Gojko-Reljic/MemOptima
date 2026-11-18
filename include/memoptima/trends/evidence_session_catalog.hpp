#pragma once

#include <memoptima/trends/trend_manifest.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace memoptima::trends {

inline constexpr std::size_t kTrendEvidenceCatalogMaximumContainerWindows = 4096U;

struct TrendContainerIdentity {
    std::string container_id;
    std::string cgroup_path;

    [[nodiscard]] auto operator<=>(const TrendContainerIdentity &) const = default;
};

// The catalog retains only temporal observation bounds needed to order sessions and reject
// ambiguity. A later analyzer rereads the selected artifacts instead of retaining every sample in
// memory.
struct TrendContainerObservationWindow {
    TrendContainerIdentity identity;
    std::string minimum_captured_at_utc;
    std::string maximum_captured_at_utc;
};

struct TrendEvidenceSession {
    std::filesystem::path artifact_path;
    std::string started_at_utc;
    std::uint64_t requested_interval_ms;
    bool incomplete;
    std::vector<TrendContainerObservationWindow> container_windows;
};

struct TrendEvidenceCatalog {
    std::vector<TrendEvidenceSession> sessions;
};

enum class TrendEvidenceCatalogErrorCode {
    artifact_validation_failed,
    ambiguous_session_overlap,
    resource_limit_exceeded,
};

struct TrendEvidenceCatalogError {
    TrendEvidenceCatalogErrorCode code;
    std::filesystem::path artifact_path;
    std::string detail;
};

// Loads selected artifacts through the shared safe validator, sorts them by session start time,
// and rejects overlapping observation windows for the same strict container identity.
[[nodiscard]] std::expected<TrendEvidenceCatalog, TrendEvidenceCatalogError>
load_trend_evidence_catalog(const TrendManifest &manifest);

} // namespace memoptima::trends
