#pragma once

#include <memoptima/analysis/cgroup_memory_assessment.hpp>
#include <memoptima/docker_container_memory_snapshot.hpp>

#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

namespace memoptima {

struct DockerContainerAssessment {
    DockerContainerId container_id;
    std::filesystem::path cgroup_path;
    analysis::CgroupMemoryAssessment assessment;
};

// Keeps discovery diagnostics beside assessments so presentation never hides incomplete evidence.
struct DockerContainerAssessmentScanResult {
    std::vector<DockerContainerAssessment> containers;
    std::size_t skipped_container_count;
    std::vector<DockerContainerScanWarning> warnings;
};

[[nodiscard]] DockerContainerAssessment
assess_docker_container_memory_snapshot(const DockerContainerMemorySnapshot &container);

[[nodiscard]] DockerContainerAssessmentScanResult
assess_docker_container_scan_result(const DockerContainerScanResult &scan_result);

} // namespace memoptima
