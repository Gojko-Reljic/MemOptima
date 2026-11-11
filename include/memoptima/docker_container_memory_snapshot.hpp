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

class DockerContainerMemorySnapshot {
  public:
    [[nodiscard]] static DockerContainerMemorySnapshot create(DockerContainerId container_id,
                                                              std::filesystem::path cgroup_path,
                                                              CgroupMemorySnapshot memory);

    [[nodiscard]] const DockerContainerId &container_id() const noexcept;
    [[nodiscard]] const std::filesystem::path &cgroup_path() const noexcept;
    [[nodiscard]] const CgroupMemorySnapshot &memory() const noexcept;

  private:
    DockerContainerMemorySnapshot(DockerContainerId container_id, std::filesystem::path cgroup_path,
                                  CgroupMemorySnapshot memory)
        : container_id_{std::move(container_id)}, cgroup_path_{std::move(cgroup_path)},
          memory_{std::move(memory)} {}

    DockerContainerId container_id_;
    std::filesystem::path cgroup_path_;
    CgroupMemorySnapshot memory_;
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

class DockerContainerScanResult {
  public:
    [[nodiscard]] static DockerContainerScanResult
    create(std::vector<DockerContainerMemorySnapshot> containers,
           std::size_t skipped_container_count, std::vector<DockerContainerScanWarning> warnings);

    [[nodiscard]] const std::vector<DockerContainerMemorySnapshot> &containers() const noexcept;
    [[nodiscard]] std::size_t skipped_container_count() const noexcept;
    [[nodiscard]] const std::vector<DockerContainerScanWarning> &warnings() const noexcept;

  private:
    DockerContainerScanResult(std::vector<DockerContainerMemorySnapshot> containers,
                              std::size_t skipped_container_count,
                              std::vector<DockerContainerScanWarning> warnings)
        : containers_{std::move(containers)}, skipped_container_count_{skipped_container_count},
          warnings_{std::move(warnings)} {}

    std::vector<DockerContainerMemorySnapshot> containers_;
    std::size_t skipped_container_count_;
    std::vector<DockerContainerScanWarning> warnings_;
};

} // namespace memoptima
