#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string format_bytes(std::uint64_t bytes);
[[nodiscard]] std::string format_percentage(double value, std::size_t decimal_places);
[[nodiscard]] std::string format_duration(std::uint64_t microseconds);
[[nodiscard]] std::string format_operational_duration(std::chrono::milliseconds duration);

} // namespace memoptima::cli
