#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace memoptima::linux::detail {

enum class MountInfoParseErrorCode {
    invalid_format,
    invalid_escape_sequence,
    cgroup_v2_not_found,
    ambiguous_cgroup_v2_mount,
};

struct MountInfoParseError {
    MountInfoParseErrorCode code;
    std::string detail;
};

[[nodiscard]] std::expected<std::filesystem::path, MountInfoParseError>
parse_cgroup_v2_mount_point(std::string_view text);

} // namespace memoptima::linux::detail
