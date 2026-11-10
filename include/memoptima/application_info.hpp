#pragma once

#include <string_view>

namespace memoptima {

[[nodiscard]] std::string_view application_name() noexcept;
[[nodiscard]] std::string_view application_description() noexcept;
[[nodiscard]] std::string_view version() noexcept;

} // namespace memoptima
