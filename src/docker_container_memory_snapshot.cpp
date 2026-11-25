#include <memoptima/docker_container_memory_snapshot.hpp>

#include "docker_container_id_syntax.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace memoptima {

std::expected<DockerContainerId, DockerContainerIdParseError>
DockerContainerId::parse(std::string_view value) {
    if (value.size() != 64U) {
        return std::unexpected(DockerContainerIdParseError{
            .code = DockerContainerIdParseErrorCode::incorrect_length,
            .detail = "A Docker container ID must contain exactly 64 hexadecimal characters.",
        });
    }

    if (!detail::is_docker_container_id_syntax(value)) {
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
