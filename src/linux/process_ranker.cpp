#include "linux/process_ranker.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace memoptima::linux::detail {

std::vector<ProcessMemoryInfo> rank_processes(std::vector<ProcessMemoryInfo> processes,
                                              std::size_t limit) {
    // Sort every readable process before truncating so the result is the global top-N set.
    std::sort(processes.begin(), processes.end(),
              [](const ProcessMemoryInfo &left, const ProcessMemoryInfo &right) {
                  if (left.resident_memory_bytes() != right.resident_memory_bytes()) {
                      return left.resident_memory_bytes() > right.resident_memory_bytes();
                  }

                  // PID makes equal VmRSS values deterministic for users and tests.
                  return left.pid() < right.pid();
              });

    if (processes.size() > limit) {
        processes.erase(processes.begin() + static_cast<std::ptrdiff_t>(limit), processes.end());
    }

    return processes;
}

} // namespace memoptima::linux::detail
