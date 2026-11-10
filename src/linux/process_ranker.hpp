#pragma once

#include <memoptima/process_memory_info.hpp>

#include <cstddef>
#include <vector>

namespace memoptima::linux::detail {

[[nodiscard]] std::vector<ProcessMemoryInfo> rank_processes(
    std::vector<ProcessMemoryInfo> processes,
    std::size_t limit);

}  // namespace memoptima::linux::detail
