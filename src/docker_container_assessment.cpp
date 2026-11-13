#include <memoptima/docker_container_assessment.hpp>

namespace memoptima {

DockerContainerAssessment
assess_docker_container_memory_snapshot(const DockerContainerMemorySnapshot &container) {
    return {
        .container_id = container.container_id,
        .cgroup_path = container.cgroup_path,
        .assessment = analysis::assess_cgroup_memory_snapshot(container.memory),
    };
}

DockerContainerAssessmentScanResult
assess_docker_container_scan_result(const DockerContainerScanResult &scan_result) {
    std::vector<DockerContainerAssessment> containers;
    containers.reserve(scan_result.containers.size());
    for (const DockerContainerMemorySnapshot &container : scan_result.containers) {
        containers.push_back(assess_docker_container_memory_snapshot(container));
    }

    return {
        .containers = std::move(containers),
        .skipped_container_count = scan_result.skipped_container_count,
        .warnings = scan_result.warnings,
    };
}

} // namespace memoptima
