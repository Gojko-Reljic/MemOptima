#pragma once

#include <memoptima/docker_container_memory_snapshot.hpp>

#include <string>
#include <vector>

namespace memoptima::cli {

[[nodiscard]] std::string
format_docker_container_scan_result(const DockerContainerScanResult &result);
[[nodiscard]] std::string
format_docker_container_scan_warnings(const DockerContainerScanResult &result);
[[nodiscard]] std::string
format_docker_container_scan_warnings(const std::vector<DockerContainerScanWarning> &warnings);
[[nodiscard]] std::string
format_docker_container_detail(const DockerContainerMemorySnapshot &container);

} // namespace memoptima::cli
