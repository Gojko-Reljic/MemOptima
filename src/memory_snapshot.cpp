#include <memoptima/memory_snapshot.hpp>

namespace memoptima {

std::expected<MemorySnapshot, SnapshotValidationError> MemorySnapshot::create(
    std::uint64_t total_memory_bytes,
    std::uint64_t available_memory_bytes,
    std::uint64_t total_swap_bytes,
    std::uint64_t free_swap_bytes) {
    if (total_memory_bytes == 0) {
        return std::unexpected(SnapshotValidationError{
            .code = SnapshotValidationErrorCode::total_memory_must_be_positive,
        });
    }

    if (available_memory_bytes > total_memory_bytes) {
        return std::unexpected(SnapshotValidationError{
            .code = SnapshotValidationErrorCode::available_memory_exceeds_total,
        });
    }

    if (free_swap_bytes > total_swap_bytes) {
        return std::unexpected(SnapshotValidationError{
            .code = SnapshotValidationErrorCode::free_swap_exceeds_total,
        });
    }

    return MemorySnapshot{
        total_memory_bytes,
        available_memory_bytes,
        total_swap_bytes,
        free_swap_bytes,
    };
}

std::uint64_t MemorySnapshot::total_memory_bytes() const noexcept {
    return total_memory_bytes_;
}

std::uint64_t MemorySnapshot::available_memory_bytes() const noexcept {
    return available_memory_bytes_;
}

std::uint64_t MemorySnapshot::total_swap_bytes() const noexcept {
    return total_swap_bytes_;
}

std::uint64_t MemorySnapshot::free_swap_bytes() const noexcept {
    return free_swap_bytes_;
}

}  // namespace memoptima
