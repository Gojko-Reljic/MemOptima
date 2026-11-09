#pragma once

#include <memoptima/memory_snapshot.hpp>

#include <cstddef>
#include <expected>
#include <string>

namespace memoptima::linux {

enum class MemoryReadErrorCode {
    file_open_failed,
    file_read_failed,
    missing_required_metric,
    duplicate_required_metric,
    invalid_value,
    unexpected_unit,
    value_overflow,
    inconsistent_values,
};

struct MemoryReadError {
    MemoryReadErrorCode code;
    std::string metric_name;
    std::size_t line_number;
    std::string detail;
};

[[nodiscard]] std::expected<MemorySnapshot, MemoryReadError> read_system_memory_snapshot();

}  // namespace memoptima::linux
