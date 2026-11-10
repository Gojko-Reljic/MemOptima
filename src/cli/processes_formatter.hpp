#pragma once

#include <memoptima/linux/process_scanner.hpp>

#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string format_process_scan_result(const linux::ProcessScanResult& result);

}  // namespace memoptima::cli
