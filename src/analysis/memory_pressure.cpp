#include <memoptima/analysis/memory_pressure.hpp>

#include <memoptima/analysis/memory_status.hpp>

namespace memoptima::analysis {
namespace {

constexpr std::uint64_t kWarningAvailableMemoryDivisor = 5;
constexpr std::uint64_t kCriticalAvailableMemoryDivisor = 10;
constexpr std::uint64_t kSwapUsageDivisor = 2;

[[nodiscard]] std::uint8_t reason_value(MemoryPressureReason reason) noexcept {
    return static_cast<std::uint8_t>(reason);
}

[[nodiscard]] bool is_at_or_below_warning_available_memory(const MemoryStatus& status) noexcept {
    return status.available_memory_bytes <= status.total_memory_bytes / kWarningAvailableMemoryDivisor;
}

[[nodiscard]] bool is_at_or_below_critical_available_memory(const MemoryStatus& status) noexcept {
    return status.available_memory_bytes <= status.total_memory_bytes / kCriticalAvailableMemoryDivisor;
}

[[nodiscard]] bool has_high_swap_usage(const MemoryStatus& status) noexcept {
    if (status.total_swap_bytes == 0) {
        return false;
    }

    const std::uint64_t half_swap_rounded_up =
        status.total_swap_bytes / kSwapUsageDivisor + status.total_swap_bytes % kSwapUsageDivisor;
    return status.used_swap_bytes >= half_swap_rounded_up;
}

[[nodiscard]] MemoryPressureLevel elevated_level(MemoryPressureLevel level) noexcept {
    switch (level) {
        case MemoryPressureLevel::Normal:
            return MemoryPressureLevel::Warning;
        case MemoryPressureLevel::Warning:
            return MemoryPressureLevel::Critical;
        case MemoryPressureLevel::Critical:
            return MemoryPressureLevel::Critical;
    }

    return MemoryPressureLevel::Critical;
}

}  // namespace

MemoryPressureLevel MemoryPressure::level() const noexcept {
    return level_;
}

bool MemoryPressure::has_reason(MemoryPressureReason reason) const noexcept {
    const std::uint8_t reason_bits = reason_value(reason);
    return reason_bits != 0 && (reasons_ & reason_bits) == reason_bits;
}

MemoryPressure evaluate_memory_pressure(const MemoryStatus& status) noexcept {
    std::uint8_t reasons = reason_value(MemoryPressureReason::None);
    MemoryPressureLevel level = MemoryPressureLevel::Normal;

    if (is_at_or_below_critical_available_memory(status)) {
        level = MemoryPressureLevel::Critical;
        reasons |= reason_value(MemoryPressureReason::AvailableMemoryAtCriticalThreshold);
    } else if (is_at_or_below_warning_available_memory(status)) {
        level = MemoryPressureLevel::Warning;
        reasons |= reason_value(MemoryPressureReason::AvailableMemoryAtWarningThreshold);
    }

    if (has_high_swap_usage(status)) {
        level = elevated_level(level);
        reasons |= reason_value(MemoryPressureReason::HighSwapUsage);
    }

    return MemoryPressure{level, reasons};
}

}  // namespace memoptima::analysis
