#pragma once

#include <memoptima/docker_container_memory_snapshot.hpp>

#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string
format_docker_container_scan_result(const DockerContainerScanResult &result);
[[nodiscard]] std::string
format_docker_container_scan_warnings(const DockerContainerScanResult &result);

} // namespace memoptima::cli
