#pragma once

#include <memoptima/trends/evidence_session_catalog.hpp>

#include <cstdint>
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

} // namespace memoptima::trends::detail
