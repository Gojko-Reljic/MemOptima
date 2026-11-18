#pragma once

#include <memoptima/trends/evidence_session_catalog.hpp>
#include <memoptima/trends/trend_analysis_service.hpp>

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace memoptima::trends::detail {

struct TrendReplaySessionMetadata {
    std::string started_at_utc;
    std::uint64_t requested_interval_ms{};
    bool incomplete{};
    std::vector<TrendContainerObservationWindow> container_windows;
};

// Returns an explanation when replay metadata no longer matches the validated catalog entry.
[[nodiscard]] std::optional<std::string>
replay_metadata_mismatch(const TrendEvidenceSession &catalog_session,
                         const TrendReplaySessionMetadata &replay_metadata);

// Replays one cataloged artifact without exposing typed evidence internals publicly.
[[nodiscard]] std::expected<void, TrendAnalysisError>
replay_trend_evidence_session(const TrendEvidenceSession &session, TrendAnalyzer &analyzer);

} // namespace memoptima::trends::detail
