#include <memoptima/analysis/memory_status.hpp>

namespace memoptima::analysis {

MemoryStatus evaluate_memory_status(const MemorySnapshot &snapshot) noexcept {
    const std::uint64_t used_memory_bytes =
        snapshot.total_memory_bytes() - snapshot.available_memory_bytes();
    const std::uint64_t used_swap_bytes = snapshot.total_swap_bytes() - snapshot.free_swap_bytes();

    const double used_memory_percentage = static_cast<double>(used_memory_bytes) /
                                          static_cast<double>(snapshot.total_memory_bytes()) *
                                          100.0;

    std::optional<double> used_swap_percentage;
    if (snapshot.total_swap_bytes() != 0) {
        used_swap_percentage = static_cast<double>(used_swap_bytes) /
                               static_cast<double>(snapshot.total_swap_bytes()) * 100.0;
    }

    return {
        .total_memory_bytes = snapshot.total_memory_bytes(),
        .available_memory_bytes = snapshot.available_memory_bytes(),
        .used_memory_bytes = used_memory_bytes,
        .used_memory_percentage = used_memory_percentage,
        .total_swap_bytes = snapshot.total_swap_bytes(),
        .free_swap_bytes = snapshot.free_swap_bytes(),
        .used_swap_bytes = used_swap_bytes,
        .used_swap_percentage = used_swap_percentage,
    };
}

} // namespace memoptima::analysis
