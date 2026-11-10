#pragma once

#include <memoptima/process_memory_info.hpp>

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

namespace memoptima::linux::detail {

enum class ProcessReadErrorCode {
    missing_required_field,
    duplicate_required_field,
    invalid_value,
    unexpected_unit,
    value_overflow,
    invalid_process_info,
};

struct ProcessReadError {
    ProcessReadErrorCode code;
    std::string field_name;
    std::size_t line_number;
    std::string detail;
};

[[nodiscard]] std::expected<ProcessMemoryInfo, ProcessReadError>
parse_process_status(std::uint32_t pid, std::string_view text);

} // namespace memoptima::linux::detail
