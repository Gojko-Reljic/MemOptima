#pragma once

#include <cstdint>
#include <expected>

namespace memoptima {

enum class SnapshotValidationErrorCode {
    total_memory_must_be_positive,
    available_memory_exceeds_total,
    free_swap_exceeds_total,
};

struct SnapshotValidationError {
    SnapshotValidationErrorCode code;
};

// A validated, immutable snapshot of the memory values required by the analysis layer.
class MemorySnapshot {
  public:
    [[nodiscard]] static std::expected<MemorySnapshot, SnapshotValidationError>
    create(std::uint64_t total_memory_bytes, std::uint64_t available_memory_bytes,
           std::uint64_t total_swap_bytes, std::uint64_t free_swap_bytes);

    [[nodiscard]] std::uint64_t total_memory_bytes() const noexcept;
    [[nodiscard]] std::uint64_t available_memory_bytes() const noexcept;
    [[nodiscard]] std::uint64_t total_swap_bytes() const noexcept;
    [[nodiscard]] std::uint64_t free_swap_bytes() const noexcept;

  private:
    constexpr MemorySnapshot(std::uint64_t total_memory_bytes, std::uint64_t available_memory_bytes,
                             std::uint64_t total_swap_bytes, std::uint64_t free_swap_bytes) noexcept
        : total_memory_bytes_{total_memory_bytes}, available_memory_bytes_{available_memory_bytes},
          total_swap_bytes_{total_swap_bytes}, free_swap_bytes_{free_swap_bytes} {}

    std::uint64_t total_memory_bytes_;
    std::uint64_t available_memory_bytes_;
    std::uint64_t total_swap_bytes_;
    std::uint64_t free_swap_bytes_;
};

} // namespace memoptima
