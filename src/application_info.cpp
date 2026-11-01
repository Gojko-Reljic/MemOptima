#include <memoptima/application_info.hpp>

#include <memoptima/version.hpp>

namespace memoptima {

std::string_view application_name() noexcept {
    return "MemOptima";
}

std::string_view application_description() noexcept {
    return "Adaptive Memory Manager for Linux";
}

std::string_view version() noexcept {
    return build::kVersion;
}

}  // namespace memoptima
