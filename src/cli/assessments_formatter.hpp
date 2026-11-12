#pragma once

#include <memoptima/docker_container_assessment.hpp>

#include <string>

namespace memoptima::cli {

[[nodiscard]] std::string
format_docker_container_assessment_scan_result(const DockerContainerAssessmentScanResult &result);
[[nodiscard]] std::string
format_docker_container_assessment_detail(const DockerContainerAssessment &assessment);

} // namespace memoptima::cli
