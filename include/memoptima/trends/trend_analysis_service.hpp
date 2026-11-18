#pragma once

#include <memoptima/trends/trend_analyzer.hpp>
#include <memoptima/trends/trend_manifest.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace memoptima::trends {

enum class TrendAnalysisErrorCode {
    catalog_failed,
    evidence_replay_failed,
    analyzer_failed,
};

struct TrendAnalysisError {
    TrendAnalysisErrorCode code;
    std::optional<std::filesystem::path> artifact_path;
    std::string detail;
};

// Creates a complete trend analysis from a manifest or returns one categorized failure.
[[nodiscard]] std::expected<TrendAnalysis, TrendAnalysisError>
analyze_trend_manifest(const TrendManifest &manifest);

} // namespace memoptima::trends
