#pragma once

#include <memoptima/trends/trend_analyzer.hpp>

#include <string>

namespace memoptima::cli {

// Formats an already analyzed manifest as the stable, human-readable v0.7 trend table.
[[nodiscard]] std::string format_trend_analysis(const trends::TrendAnalysis &analysis);

} // namespace memoptima::cli
