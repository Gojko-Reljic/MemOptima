#pragma once

#include <memoptima/process_memory_info.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace memoptima::linux {

enum class ProcessScanErrorCode {
    proc_directory_open_failed,
    directory_iteration_failed,
};

struct ProcessScanError {
    ProcessScanErrorCode code;
    std::string detail;
};

struct ProcessScanResult {
    std::vector<ProcessMemoryInfo> processes;
    std::size_t skipped_process_count;
};

[[nodiscard]] std::expected<ProcessScanResult, ProcessScanError> scan_process_memory();

}  // namespace memoptima::linux
