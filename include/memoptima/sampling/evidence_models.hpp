#pragma once

#include <memoptima/analysis/cgroup_memory_assessment.hpp>
#include <memoptima/docker_container_memory_snapshot.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include <chrono>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::sampling {

enum class SamplingSessionOutcome {
    completed,
    interrupted,
    failed,
};

enum class SamplingSessionFailureCode {
    output_size_limit_reached,
    output_write_failed,
    output_flush_failed,
};

struct SamplingSessionFailure {
    SamplingSessionFailureCode code;
    std::string detail;
};

class DockerContainerEvidence {
  public:
    // Stores the raw measurement with the assessment derived from that exact measurement.
    [[nodiscard]] static DockerContainerEvidence create(DockerContainerMemorySnapshot snapshot);

    [[nodiscard]] const DockerContainerMemorySnapshot &snapshot() const noexcept;
    [[nodiscard]] const analysis::CgroupMemoryAssessment &assessment() const noexcept;

  private:
    DockerContainerEvidence(DockerContainerMemorySnapshot snapshot,
                            analysis::CgroupMemoryAssessment assessment)
        : snapshot_{std::move(snapshot)}, assessment_{std::move(assessment)} {}

    DockerContainerMemorySnapshot snapshot_;
    analysis::CgroupMemoryAssessment assessment_;
};

class SamplingEvidenceSample {
  public:
    [[nodiscard]] static SamplingEvidenceSample
    create(SamplingCount sequence, std::chrono::milliseconds planned_offset,
           std::chrono::system_clock::time_point captured_at,
           std::chrono::milliseconds schedule_delay,
           std::vector<DockerContainerEvidence> containers, std::size_t skipped_container_count,
           std::vector<DockerContainerScanWarning> discovery_warnings);

    [[nodiscard]] SamplingCount sequence() const noexcept;
    [[nodiscard]] std::chrono::milliseconds planned_offset() const noexcept;
    [[nodiscard]] std::chrono::system_clock::time_point captured_at() const noexcept;
    [[nodiscard]] std::chrono::milliseconds schedule_delay() const noexcept;
    [[nodiscard]] const std::vector<DockerContainerEvidence> &containers() const noexcept;
    [[nodiscard]] std::size_t skipped_container_count() const noexcept;
    [[nodiscard]] const std::vector<DockerContainerScanWarning> &
    discovery_warnings() const noexcept;

  private:
    SamplingEvidenceSample(SamplingCount sequence, std::chrono::milliseconds planned_offset,
                           std::chrono::system_clock::time_point captured_at,
                           std::chrono::milliseconds schedule_delay,
                           std::vector<DockerContainerEvidence> containers,
                           std::size_t skipped_container_count,
                           std::vector<DockerContainerScanWarning> discovery_warnings)
        : sequence_{sequence}, planned_offset_{planned_offset}, captured_at_{captured_at},
          schedule_delay_{schedule_delay}, containers_{std::move(containers)},
          skipped_container_count_{skipped_container_count},
          discovery_warnings_{std::move(discovery_warnings)} {}

    SamplingCount sequence_;
    std::chrono::milliseconds planned_offset_;
    std::chrono::system_clock::time_point captured_at_;
    std::chrono::milliseconds schedule_delay_;
    std::vector<DockerContainerEvidence> containers_;
    std::size_t skipped_container_count_;
    std::vector<DockerContainerScanWarning> discovery_warnings_;
};

} // namespace memoptima::sampling
