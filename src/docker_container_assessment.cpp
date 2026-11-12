#include <memoptima/docker_container_assessment.hpp>

namespace memoptima {

DockerContainerAssessment
DockerContainerAssessment::create(DockerContainerId container_id, std::filesystem::path cgroup_path,
                                  analysis::CgroupMemoryAssessment assessment) {
    return DockerContainerAssessment{std::move(container_id), std::move(cgroup_path),
                                     std::move(assessment)};
}

const DockerContainerId &DockerContainerAssessment::container_id() const noexcept {
    return container_id_;
}

const std::filesystem::path &DockerContainerAssessment::cgroup_path() const noexcept {
    return cgroup_path_;
}

const analysis::CgroupMemoryAssessment &DockerContainerAssessment::assessment() const noexcept {
    return assessment_;
}

DockerContainerAssessmentScanResult
DockerContainerAssessmentScanResult::create(std::vector<DockerContainerAssessment> containers,
                                            std::size_t skipped_container_count,
                                            std::vector<DockerContainerScanWarning> warnings) {
    return DockerContainerAssessmentScanResult{std::move(containers), skipped_container_count,
                                               std::move(warnings)};
}

const std::vector<DockerContainerAssessment> &
DockerContainerAssessmentScanResult::containers() const noexcept {
    return containers_;
}

std::size_t DockerContainerAssessmentScanResult::skipped_container_count() const noexcept {
    return skipped_container_count_;
}

const std::vector<DockerContainerScanWarning> &
DockerContainerAssessmentScanResult::warnings() const noexcept {
    return warnings_;
}

DockerContainerAssessment
assess_docker_container_memory_snapshot(const DockerContainerMemorySnapshot &container) {
    return DockerContainerAssessment::create(
        container.container_id(), container.cgroup_path(),
        analysis::assess_cgroup_memory_snapshot(container.memory()));
}

DockerContainerAssessmentScanResult
assess_docker_container_scan_result(const DockerContainerScanResult &scan_result) {
    std::vector<DockerContainerAssessment> containers;
    containers.reserve(scan_result.containers().size());
    for (const DockerContainerMemorySnapshot &container : scan_result.containers()) {
        containers.push_back(assess_docker_container_memory_snapshot(container));
    }

    return DockerContainerAssessmentScanResult::create(
        std::move(containers), scan_result.skipped_container_count(), scan_result.warnings());
}

} // namespace memoptima
