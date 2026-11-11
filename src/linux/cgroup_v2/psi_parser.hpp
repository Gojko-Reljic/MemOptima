#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace memoptima::linux::detail {

struct PsiPressureLine {
    double average_10_seconds;
    double average_60_seconds;
    double average_300_seconds;
    std::uint64_t total_microseconds;
};

struct PsiMemoryPressure {
    PsiPressureLine some;
    PsiPressureLine full;
};

enum class PsiParseErrorCode {
    invalid_format,
    duplicate_pressure_type,
    missing_pressure_type,
    missing_required_field,
    duplicate_required_field,
    invalid_value,
    value_out_of_range,
    value_overflow,
};

struct PsiParseError {
    PsiParseErrorCode code;
    std::string detail;
};

[[nodiscard]] std::expected<PsiMemoryPressure, PsiParseError>
parse_memory_pressure(std::string_view text);

} // namespace memoptima::linux::detail
