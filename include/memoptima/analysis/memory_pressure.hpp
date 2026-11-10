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
    None = 0,
    LowAvailableMemory = 1U << 0U,
    HighSwapUsage = 1U << 1U,
};

class MemoryPressure {
public:
    [[nodiscard]] MemoryPressureLevel level() const noexcept;
    [[nodiscard]] bool has_reason(MemoryPressureReason reason) const noexcept;

private:
    constexpr MemoryPressure(MemoryPressureLevel level, std::uint8_t reasons) noexcept
        : level_{level}, reasons_{reasons} {}

    friend MemoryPressure evaluate_memory_pressure(const MemoryStatus& status) noexcept;

    MemoryPressureLevel level_;
    std::uint8_t reasons_;
};

[[nodiscard]] MemoryPressure evaluate_memory_pressure(const MemoryStatus& status) noexcept;

}  // namespace memoptima::analysis
