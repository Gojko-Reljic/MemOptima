#include "linux/cgroup_v2/mountinfo_parser.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace memoptima::linux::detail {
namespace {

[[nodiscard]] MountInfoParseError make_error(MountInfoParseErrorCode code,
                                             std::string_view detail) {
    return {
        .code = code,
        .detail = std::string{detail},
    };
}

[[nodiscard]] bool is_ascii_whitespace(char character) noexcept {
    return character == ' ' || character == '\t' || character == '\r';
}

[[nodiscard]] std::vector<std::string_view> split_ascii_whitespace(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t position{};

    while (position < text.size()) {
        while (position < text.size() && is_ascii_whitespace(text[position])) {
            ++position;
        }

        const std::size_t field_start = position;
        while (position < text.size() && !is_ascii_whitespace(text[position])) {
            ++position;
        }

        if (field_start != position) {
            fields.push_back(text.substr(field_start, position - field_start));
        }
    }

    return fields;
}

[[nodiscard]] bool is_octal_digit(char character) noexcept {
    return character >= '0' && character <= '7';
}

[[nodiscard]] std::expected<std::string, MountInfoParseError>
decode_mountinfo_path(std::string_view encoded_path) {
    std::string decoded_path;
    decoded_path.reserve(encoded_path.size());

    for (std::size_t position{}; position < encoded_path.size(); ++position) {
        if (encoded_path[position] != '\\') {
            decoded_path.push_back(encoded_path[position]);
            continue;
        }

        if (position + 3 >= encoded_path.size() || !is_octal_digit(encoded_path[position + 1]) ||
            !is_octal_digit(encoded_path[position + 2]) ||
            !is_octal_digit(encoded_path[position + 3])) {
            return std::unexpected(make_error(MountInfoParseErrorCode::invalid_escape_sequence,
                                              "Mount point contains an invalid escape sequence."));
        }

        const unsigned char decoded_character = static_cast<unsigned char>(
            ((encoded_path[position + 1] - '0') << 6U) |
            ((encoded_path[position + 2] - '0') << 3U) | (encoded_path[position + 3] - '0'));
        decoded_path.push_back(static_cast<char>(decoded_character));
        position += 3;
    }

    return decoded_path;
}

} // namespace

std::expected<std::filesystem::path, MountInfoParseError>
parse_cgroup_v2_mount_point(std::string_view text) {
    std::vector<std::filesystem::path> cgroup_v2_roots;
    std::size_t line_start{};

    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line = text.substr(line_start, line_end - line_start);
        const std::size_t separator = line.find(" - ");

        if (separator == std::string_view::npos) {
            return std::unexpected(make_error(MountInfoParseErrorCode::invalid_format,
                                              "Mount information line is missing the separator."));
        }

        const std::vector<std::string_view> left_fields =
            split_ascii_whitespace(line.substr(0, separator));
        const std::vector<std::string_view> right_fields =
            split_ascii_whitespace(line.substr(separator + 3));

        if (left_fields.size() < 6 || right_fields.size() < 3) {
            return std::unexpected(make_error(MountInfoParseErrorCode::invalid_format,
                                              "Mount information line has too few fields."));
        }

        if (right_fields[0] == "cgroup2" && left_fields[3] == "/") {
            const auto mount_point = decode_mountinfo_path(left_fields[4]);
            if (!mount_point.has_value()) {
                return std::unexpected(mount_point.error());
            }

            cgroup_v2_roots.emplace_back(*mount_point);
        }

        if (line_end == std::string_view::npos) {
            break;
        }

        line_start = line_end + 1;
    }

    if (cgroup_v2_roots.empty()) {
        return std::unexpected(make_error(MountInfoParseErrorCode::cgroup_v2_not_found,
                                          "No unified cgroup v2 root mount was found."));
    }

    if (cgroup_v2_roots.size() != 1) {
        return std::unexpected(make_error(MountInfoParseErrorCode::ambiguous_cgroup_v2_mount,
                                          "More than one unified cgroup v2 root mount was found."));
    }

    return cgroup_v2_roots.front();
}

} // namespace memoptima::linux::detail
