#pragma once

#include <memoptima/forecast_evaluation/forecast_evaluation_analyzer.hpp>

#include <string>

namespace memoptima::cli {

// Formats a complete v0.8 forecast evaluation as a stable human-readable summary and table.
[[nodiscard]] std::string
format_forecast_evaluation(const forecast_evaluation::ForecastEvaluationReport &report);

} // namespace memoptima::cli
