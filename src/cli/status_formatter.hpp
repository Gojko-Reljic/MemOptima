#pragma once

#include <memoptima/analysis/memory_pressure.hpp>
#include <memoptima/analysis/memory_status.hpp>

#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string format_memory_status(const analysis::MemoryStatus &status,
                                               const analysis::MemoryPressure &pressure);

} // namespace memoptima::cli
