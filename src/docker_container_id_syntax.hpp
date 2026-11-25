#pragma once

#include <cstddef>
#include <string_view>

namespace memoptima::detail {

[[nodiscard]] constexpr bool is_docker_container_id_syntax(std::string_view value) noexcept {
    if (value.size() != 64U)
        return false;

    for (const unsigned char character : value)
        if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f') ||
              (character >= 'A' && character <= 'F')))
            return false;
    return true;
}

} // namespace memoptima::detail
