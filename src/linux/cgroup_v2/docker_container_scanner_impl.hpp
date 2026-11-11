#pragma once

#include <memoptima/docker_container_memory_snapshot.hpp>

#include <filesystem>

namespace memoptima::linux::detail {

[[nodiscard]] DockerContainerScanResult
scan_rootful_docker_containers_from_root(const std::filesystem::path &cgroup_root);

} // namespace memoptima::linux::detail
