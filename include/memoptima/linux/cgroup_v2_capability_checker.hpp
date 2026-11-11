#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <utility>

namespace memoptima::linux {

enum class CgroupV2CapabilityErrorCode {
    mount_info_open_failed,
    mount_info_read_failed,
    mount_info_invalid_format,
    cgroup_v2_not_mounted,
    ambiguous_cgroup_v2_mount,
    cgroup_controllers_open_failed,
    cgroup_controllers_read_failed,
    memory_controller_not_available,
    host_memory_psi_open_failed,
    host_memory_psi_read_failed,
    host_memory_psi_invalid_format,
};

struct CgroupV2CapabilityError {
    CgroupV2CapabilityErrorCode code;
    std::string detail;
};

class CgroupV2Environment {
  public:
    [[nodiscard]] const std::filesystem::path &cgroup_root() const noexcept;

  private:
    explicit CgroupV2Environment(std::filesystem::path cgroup_root)
        : cgroup_root_{std::move(cgroup_root)} {}

    std::filesystem::path cgroup_root_;

    friend std::expected<CgroupV2Environment, CgroupV2CapabilityError>
    detect_cgroup_v2_environment();
};

[[nodiscard]] std::expected<CgroupV2Environment, CgroupV2CapabilityError>
detect_cgroup_v2_environment();

} // namespace memoptima::linux
