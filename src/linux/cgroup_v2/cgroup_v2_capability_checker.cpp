#include <memoptima/linux/cgroup_v2_capability_checker.hpp>

#include "linux/cgroup_v2/mountinfo_parser.hpp"
#include "linux/cgroup_v2/psi_parser.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace memoptima::linux {
namespace {

constexpr std::string_view kMountInfoPath{"/proc/self/mountinfo"};
constexpr std::string_view kHostMemoryPressurePath{"/proc/pressure/memory"};

[[nodiscard]] CgroupV2CapabilityError make_error(CgroupV2CapabilityErrorCode code,
                                                 std::string_view detail) {
    return {
        .code = code,
        .detail = std::string{detail},
    };
}

[[nodiscard]] std::expected<std::string, CgroupV2CapabilityError>
read_file(const std::filesystem::path &path, CgroupV2CapabilityErrorCode open_error,
          CgroupV2CapabilityErrorCode read_error) {
    std::ifstream file{path};
    if (!file.is_open()) {
        return std::unexpected(make_error(open_error, "Unable to open " + path.string() + '.'));
    }

    const std::string contents{std::istreambuf_iterator<char>{file},
                               std::istreambuf_iterator<char>{}};
    if (file.bad()) {
        return std::unexpected(make_error(read_error, "Unable to read " + path.string() + '.'));
    }

    return contents;
}

[[nodiscard]] bool contains_memory_controller(std::string_view controllers) noexcept {
    std::size_t position{};
    while (position < controllers.size()) {
        while (position < controllers.size() &&
               (controllers[position] == ' ' || controllers[position] == '\t' ||
                controllers[position] == '\r' || controllers[position] == '\n')) {
            ++position;
        }

        const std::size_t controller_start = position;
        while (position < controllers.size() && controllers[position] != ' ' &&
               controllers[position] != '\t' && controllers[position] != '\r' &&
               controllers[position] != '\n') {
            ++position;
        }

        if (controllers.substr(controller_start, position - controller_start) == "memory") {
            return true;
        }
    }

    return false;
}

} // namespace

const std::filesystem::path &CgroupV2Environment::cgroup_root() const noexcept {
    return cgroup_root_;
}

std::expected<CgroupV2Environment, CgroupV2CapabilityError> detect_cgroup_v2_environment() {
    const auto mount_info =
        read_file(kMountInfoPath, CgroupV2CapabilityErrorCode::mount_info_open_failed,
                  CgroupV2CapabilityErrorCode::mount_info_read_failed);
    if (!mount_info.has_value()) {
        return std::unexpected(mount_info.error());
    }

    const auto cgroup_root = detail::parse_cgroup_v2_mount_point(*mount_info);
    if (!cgroup_root.has_value()) {
        switch (cgroup_root.error().code) {
        case detail::MountInfoParseErrorCode::cgroup_v2_not_found:
            return std::unexpected(make_error(CgroupV2CapabilityErrorCode::cgroup_v2_not_mounted,
                                              cgroup_root.error().detail));
        case detail::MountInfoParseErrorCode::ambiguous_cgroup_v2_mount:
            return std::unexpected(
                make_error(CgroupV2CapabilityErrorCode::ambiguous_cgroup_v2_mount,
                           cgroup_root.error().detail));
        case detail::MountInfoParseErrorCode::invalid_format:
        case detail::MountInfoParseErrorCode::invalid_escape_sequence:
            return std::unexpected(
                make_error(CgroupV2CapabilityErrorCode::mount_info_invalid_format,
                           cgroup_root.error().detail));
        }
    }

    const auto controllers = read_file(*cgroup_root / "cgroup.controllers",
                                       CgroupV2CapabilityErrorCode::cgroup_controllers_open_failed,
                                       CgroupV2CapabilityErrorCode::cgroup_controllers_read_failed);
    if (!controllers.has_value()) {
        return std::unexpected(controllers.error());
    }

    if (!contains_memory_controller(*controllers)) {
        return std::unexpected(
            make_error(CgroupV2CapabilityErrorCode::memory_controller_not_available,
                       "The cgroup v2 memory controller is not available."));
    }

    const auto host_memory_pressure =
        read_file(kHostMemoryPressurePath, CgroupV2CapabilityErrorCode::host_memory_psi_open_failed,
                  CgroupV2CapabilityErrorCode::host_memory_psi_read_failed);
    if (!host_memory_pressure.has_value()) {
        return std::unexpected(host_memory_pressure.error());
    }

    const auto parsed_pressure = detail::parse_memory_pressure(*host_memory_pressure);
    if (!parsed_pressure.has_value()) {
        return std::unexpected(
            make_error(CgroupV2CapabilityErrorCode::host_memory_psi_invalid_format,
                       parsed_pressure.error().detail));
    }

    return CgroupV2Environment{*cgroup_root};
}

} // namespace memoptima::linux
