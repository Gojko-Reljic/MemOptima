#include <memoptima/docker_container_memory_snapshot.hpp>

#include <algorithm>
#include <cctype>
#include <string>

namespace memoptima {
namespace {

constexpr std::size_t kDockerContainerIdLength = 64;

[[nodiscard]] bool is_hexadecimal_character(char value) noexcept {
    const unsigned char character = static_cast<unsigned char>(value);
    return std::isxdigit(character) != 0;
}

} // namespace

std::expected<DockerContainerId, DockerContainerIdParseError>
DockerContainerId::parse(std::string_view value) {
    if (value.size() != kDockerContainerIdLength) {
        return std::unexpected(DockerContainerIdParseError{
            .code = DockerContainerIdParseErrorCode::incorrect_length,
            .detail = "A Docker container ID must contain exactly 64 hexadecimal characters.",
        });
    }

    if (!std::ranges::all_of(value, is_hexadecimal_character)) {
        return std::unexpected(DockerContainerIdParseError{
            .code = DockerContainerIdParseErrorCode::invalid_character,
            .detail = "A Docker container ID can contain only hexadecimal characters.",
        });
    }

    std::string normalized{value};
    std::ranges::transform(normalized, normalized.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return DockerContainerId{std::move(normalized)};
}

const std::string &DockerContainerId::value() const noexcept { return value_; }

} // namespace memoptima
