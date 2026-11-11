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

DockerContainerMemorySnapshot
DockerContainerMemorySnapshot::create(DockerContainerId container_id,
                                      std::filesystem::path cgroup_path,
                                      CgroupMemorySnapshot memory) {
    return DockerContainerMemorySnapshot{std::move(container_id), std::move(cgroup_path),
                                         std::move(memory)};
}

const DockerContainerId &DockerContainerMemorySnapshot::container_id() const noexcept {
    return container_id_;
}

const std::filesystem::path &DockerContainerMemorySnapshot::cgroup_path() const noexcept {
    return cgroup_path_;
}

const CgroupMemorySnapshot &DockerContainerMemorySnapshot::memory() const noexcept {
    return memory_;
}

DockerContainerScanResult
DockerContainerScanResult::create(std::vector<DockerContainerMemorySnapshot> containers,
                                  std::size_t skipped_container_count,
                                  std::vector<DockerContainerScanWarning> warnings) {
    return DockerContainerScanResult{std::move(containers), skipped_container_count,
                                     std::move(warnings)};
}

const std::vector<DockerContainerMemorySnapshot> &
DockerContainerScanResult::containers() const noexcept {
    return containers_;
}

std::size_t DockerContainerScanResult::skipped_container_count() const noexcept {
    return skipped_container_count_;
}

const std::vector<DockerContainerScanWarning> &
DockerContainerScanResult::warnings() const noexcept {
    return warnings_;
}

} // namespace memoptima
