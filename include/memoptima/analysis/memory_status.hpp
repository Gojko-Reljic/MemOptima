#pragma once

#include <memoptima/memory_snapshot.hpp>

#include <cstdint>
#include <optional>

namespace memoptima::analysis {

struct MemoryStatus {
    std::uint64_t total_memory_bytes;
    std::uint64_t available_memory_bytes;
    std::uint64_t used_memory_bytes;
    double used_memory_percentage;

    std::uint64_t total_swap_bytes;
    std::uint64_t free_swap_bytes;
    std::uint64_t used_swap_bytes;
    std::optional<double> used_swap_percentage;
};

[[nodiscard]] MemoryStatus evaluate_memory_status(const MemorySnapshot& snapshot) noexcept;

}  // namespace memoptima::analysis
