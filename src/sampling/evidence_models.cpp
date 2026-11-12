#include <memoptima/sampling/evidence_models.hpp>

namespace memoptima::sampling {

DockerContainerEvidence DockerContainerEvidence::create(DockerContainerMemorySnapshot snapshot) {
    const analysis::CgroupMemoryAssessment assessment =
        analysis::assess_cgroup_memory_snapshot(snapshot.memory());
    return DockerContainerEvidence{std::move(snapshot), assessment};
}

const DockerContainerMemorySnapshot &DockerContainerEvidence::snapshot() const noexcept {
    return snapshot_;
}

const analysis::CgroupMemoryAssessment &DockerContainerEvidence::assessment() const noexcept {
    return assessment_;
}

SamplingEvidenceSample SamplingEvidenceSample::create(
    SamplingCount sequence, std::chrono::milliseconds planned_offset,
    std::chrono::system_clock::time_point captured_at, std::chrono::milliseconds schedule_delay,
    std::vector<DockerContainerEvidence> containers, std::size_t skipped_container_count,
    std::vector<DockerContainerScanWarning> discovery_warnings) {
    return SamplingEvidenceSample{sequence,
                                  planned_offset,
                                  captured_at,
                                  schedule_delay,
                                  std::move(containers),
                                  skipped_container_count,
                                  std::move(discovery_warnings)};
}

SamplingCount SamplingEvidenceSample::sequence() const noexcept { return sequence_; }

std::chrono::milliseconds SamplingEvidenceSample::planned_offset() const noexcept {
    return planned_offset_;
}

std::chrono::system_clock::time_point SamplingEvidenceSample::captured_at() const noexcept {
    return captured_at_;
}

std::chrono::milliseconds SamplingEvidenceSample::schedule_delay() const noexcept {
    return schedule_delay_;
}

const std::vector<DockerContainerEvidence> &SamplingEvidenceSample::containers() const noexcept {
    return containers_;
}

std::size_t SamplingEvidenceSample::skipped_container_count() const noexcept {
    return skipped_container_count_;
}

const std::vector<DockerContainerScanWarning> &
SamplingEvidenceSample::discovery_warnings() const noexcept {
    return discovery_warnings_;
}

} // namespace memoptima::sampling
