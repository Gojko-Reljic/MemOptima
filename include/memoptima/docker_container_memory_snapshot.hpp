#pragma once

#include <memoptima/cgroup_memory_snapshot.hpp>

#include <cstddef>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace memoptima {

enum class DockerContainerIdParseErrorCode {
    incorrect_length,
    invalid_character,
};

struct DockerContainerIdParseError {
    DockerContainerIdParseErrorCode code;
    std::string detail;
};

class DockerContainerId {
  public:
    [[nodiscard]] static std::expected<DockerContainerId, DockerContainerIdParseError>
    parse(std::string_view value);

    [[nodiscard]] const std::string &value() const noexcept;

  private:
    explicit DockerContainerId(std::string value) : value_{std::move(value)} {}

    std::string value_;
};

struct DockerContainerMemorySnapshot {
    DockerContainerId container_id;
    std::filesystem::path cgroup_path;
    CgroupMemorySnapshot memory;
};

enum class DockerContainerScanWarningCode {
    docker_cgroup_layout_not_found,
    cgroup_directory_open_failed,
    cgroup_directory_read_failed,
    invalid_container_identifier,
    duplicate_container_identifier,
};

struct DockerContainerScanWarning {
    DockerContainerScanWarningCode code;
    std::filesystem::path cgroup_path;
    std::string detail;
};

struct DockerContainerScanResult {
    std::vector<DockerContainerMemorySnapshot> containers;
    std::size_t skipped_container_count;
    std::vector<DockerContainerScanWarning> warnings;
};

} // namespace memoptima
