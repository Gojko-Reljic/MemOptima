#pragma once

#include <cstdint>

namespace memoptima::analysis {

struct MemoryStatus;

enum class MemoryPressureLevel {
    Normal,
    Warning,
    Critical,
};

enum class MemoryPressureReason : std::uint8_t {
    // Bit flags allow a pressure result to report more than one active reason.
    None = 0,
    AvailableMemoryAtWarningThreshold = 1U << 0U,
    AvailableMemoryAtCriticalThreshold = 1U << 1U,
    HighSwapUsage = 1U << 2U,
};

class MemoryPressure {
  public:
    [[nodiscard]] MemoryPressureLevel level() const noexcept;
    [[nodiscard]] bool has_reason(MemoryPressureReason reason) const noexcept;

  private:
    constexpr MemoryPressure(MemoryPressureLevel level, std::uint8_t reasons) noexcept
        : level_{level}, reasons_{reasons} {}

    friend MemoryPressure evaluate_memory_pressure(const MemoryStatus &status) noexcept;

    MemoryPressureLevel level_;
    std::uint8_t reasons_;
};

[[nodiscard]] MemoryPressure evaluate_memory_pressure(const MemoryStatus &status) noexcept;

} // namespace memoptima::analysis
