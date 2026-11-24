#pragma once

#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>
#include <memoptima/trends/trend_manifest.hpp>

#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace memoptima::forecast_evaluation {

enum class ForecastEvaluationErrorCode {
    catalog_failed,
    evidence_replay_failed,
    analyzer_failed,
};

struct ForecastEvaluationError {
    ForecastEvaluationErrorCode code;
    std::optional<std::filesystem::path> artifact_path;
    std::string detail;
};

// Creates a complete forecast evaluation from a validated manifest or returns one categorized
// failure without exposing a partial report.
[[nodiscard]] std::expected<ForecastEvaluationReport, ForecastEvaluationError>
evaluate_forecast_manifest(const trends::TrendManifest &manifest);

} // namespace memoptima::forecast_evaluation
