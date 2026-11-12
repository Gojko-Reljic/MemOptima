#pragma once

#include <memoptima/analysis/cgroup_memory_assessment.hpp>
#include <memoptima/docker_container_memory_snapshot.hpp>

#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

namespace memoptima {

class DockerContainerAssessment {
  public:
    [[nodiscard]] static DockerContainerAssessment
    create(DockerContainerId container_id, std::filesystem::path cgroup_path,
           analysis::CgroupMemoryAssessment assessment);

    [[nodiscard]] const DockerContainerId &container_id() const noexcept;
    [[nodiscard]] const std::filesystem::path &cgroup_path() const noexcept;
    [[nodiscard]] const analysis::CgroupMemoryAssessment &assessment() const noexcept;

  private:
    DockerContainerAssessment(DockerContainerId container_id, std::filesystem::path cgroup_path,
                              analysis::CgroupMemoryAssessment assessment)
        : container_id_{std::move(container_id)}, cgroup_path_{std::move(cgroup_path)},
          assessment_{std::move(assessment)} {}

    DockerContainerId container_id_;
    std::filesystem::path cgroup_path_;
    analysis::CgroupMemoryAssessment assessment_;
};

class DockerContainerAssessmentScanResult {
  public:
    [[nodiscard]] static DockerContainerAssessmentScanResult
    create(std::vector<DockerContainerAssessment> containers, std::size_t skipped_container_count,
           std::vector<DockerContainerScanWarning> warnings);

    [[nodiscard]] const std::vector<DockerContainerAssessment> &containers() const noexcept;
    [[nodiscard]] std::size_t skipped_container_count() const noexcept;
    [[nodiscard]] const std::vector<DockerContainerScanWarning> &warnings() const noexcept;

  private:
    DockerContainerAssessmentScanResult(std::vector<DockerContainerAssessment> containers,
                                        std::size_t skipped_container_count,
                                        std::vector<DockerContainerScanWarning> warnings)
        : containers_{std::move(containers)}, skipped_container_count_{skipped_container_count},
          warnings_{std::move(warnings)} {}

    std::vector<DockerContainerAssessment> containers_;
    std::size_t skipped_container_count_;
    std::vector<DockerContainerScanWarning> warnings_;
};

[[nodiscard]] DockerContainerAssessment
assess_docker_container_memory_snapshot(const DockerContainerMemorySnapshot &container);

[[nodiscard]] DockerContainerAssessmentScanResult
assess_docker_container_scan_result(const DockerContainerScanResult &scan_result);

} // namespace memoptima
