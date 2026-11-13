#pragma once

namespace memoptima::linux::detail {

[[nodiscard]] constexpr bool is_ascii_whitespace(char character) noexcept {
    return character == ' ' || character == '\t' || character == '\r';
}

} // namespace memoptima::linux::detail
