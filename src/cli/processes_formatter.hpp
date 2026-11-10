#pragma once

#include <memoptima/linux/process_scanner.hpp>

#include <string>

namespace memoptima::cli {

// Formats scan results only; collecting and ranking processes remain Linux-layer responsibilities.
[[nodiscard]] std::string format_process_scan_result(const linux::ProcessScanResult &result);

} // namespace memoptima::cli
