#pragma once

#include <memoptima/forecast/evidence_forecast.hpp>

#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string
format_evidence_forecast(const forecast::EvidenceForecastSummary &summary);

} // namespace memoptima::cli
