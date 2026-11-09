#pragma once

#include <memoptima/linux/memory_reader.hpp>

#include <expected>
#include <string_view>

namespace memoptima::linux::detail {

[[nodiscard]] std::expected<MemorySnapshot, MemoryReadError> parse_memory_info(std::string_view text);

}  // namespace memoptima::linux::detail
