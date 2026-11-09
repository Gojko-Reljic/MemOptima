#pragma once

#include <cstdint>

namespace memoptima {

struct MemorySnapshot {
    std::uint64_t total_memory_bytes;
    std::uint64_t available_memory_bytes;
    std::uint64_t total_swap_bytes;
    std::uint64_t free_swap_bytes;
};

}  // namespace memoptima
