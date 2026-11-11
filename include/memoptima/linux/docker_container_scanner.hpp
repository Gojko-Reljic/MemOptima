#pragma once

#include <memoptima/docker_container_memory_snapshot.hpp>
#include <memoptima/linux/cgroup_v2_capability_checker.hpp>

namespace memoptima::linux {

[[nodiscard]] DockerContainerScanResult
scan_rootful_docker_containers(const CgroupV2Environment &environment);

} // namespace memoptima::linux
