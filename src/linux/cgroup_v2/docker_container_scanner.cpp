#include <memoptima/linux/docker_container_scanner.hpp>

#include "linux/cgroup_v2/cgroup_memory_reader_impl.hpp"
#include "linux/cgroup_v2/docker_container_scanner_impl.hpp"

#include <map>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace memoptima::linux::detail {
namespace {

enum class DockerCgroupLayout {
    cgroupfs,
    systemd,
};

struct DockerCgroupLocation {
    std::filesystem::path relative_parent_path;
    DockerCgroupLayout layout;
};

struct DiscoveredContainer {
    DockerContainerMemorySnapshot snapshot;
    DockerCgroupLayout layout;
};

const DockerCgroupLocation kDockerCgroupLocations[]{
    {.relative_parent_path = "docker", .layout = DockerCgroupLayout::cgroupfs},
    {.relative_parent_path = "system.slice", .layout = DockerCgroupLayout::systemd},
};

[[nodiscard]] bool is_systemd_container_directory(std::string_view name) noexcept {
    constexpr std::string_view prefix{"docker-"};
    constexpr std::string_view suffix{".scope"};
    return name.starts_with(prefix) && name.ends_with(suffix) &&
           name.size() > prefix.size() + suffix.size();
}

[[nodiscard]] std::string_view systemd_container_id_text(std::string_view name) noexcept {
    constexpr std::string_view prefix{"docker-"};
    constexpr std::string_view suffix{".scope"};
    return name.substr(prefix.size(), name.size() - prefix.size() - suffix.size());
}

[[nodiscard]] bool is_higher_priority(DockerCgroupLayout layout,
                                      DockerCgroupLayout other_layout) noexcept {
    return layout == DockerCgroupLayout::systemd && other_layout == DockerCgroupLayout::cgroupfs;
}

[[nodiscard]] DockerContainerScanWarning make_warning(DockerContainerScanWarningCode code,
                                                      const std::filesystem::path &path,
                                                      std::string detail) {
    return {.code = code, .cgroup_path = path, .detail = std::move(detail)};
}

void add_candidate(const std::filesystem::path &cgroup_root,
                   const std::filesystem::path &relative_cgroup_path, DockerCgroupLayout layout,
                   std::string_view container_id_text,
                   std::map<std::string, DiscoveredContainer> &containers,
                   std::size_t &skipped_container_count,
                   std::vector<DockerContainerScanWarning> &warnings) {
    const std::filesystem::path absolute_cgroup_path = cgroup_root / relative_cgroup_path;
    const auto container_id = DockerContainerId::parse(container_id_text);
    if (!container_id.has_value()) {
        ++skipped_container_count;
        warnings.push_back(make_warning(
            DockerContainerScanWarningCode::invalid_container_identifier, absolute_cgroup_path,
            "Skipping cgroup directory because its Docker container ID is invalid: " +
                container_id.error().detail));
        return;
    }

    DockerContainerMemorySnapshot snapshot = DockerContainerMemorySnapshot::create(
        *container_id, absolute_cgroup_path,
        read_cgroup_memory_snapshot_from_root(cgroup_root, relative_cgroup_path));
    const auto existing = containers.find(container_id->value());
    if (existing == containers.end()) {
        containers.emplace(container_id->value(), DiscoveredContainer{std::move(snapshot), layout});
        return;
    }

    warnings.push_back(make_warning(
        DockerContainerScanWarningCode::duplicate_container_identifier, absolute_cgroup_path,
        "The Docker container ID is present in more than one standard cgroup location."));
    if (is_higher_priority(layout, existing->second.layout)) {
        existing->second = DiscoveredContainer{std::move(snapshot), layout};
    }
}

void scan_location(const std::filesystem::path &cgroup_root, const DockerCgroupLocation &location,
                   std::map<std::string, DiscoveredContainer> &containers,
                   std::size_t &skipped_container_count,
                   std::vector<DockerContainerScanWarning> &warnings, bool &found_layout) {
    const std::filesystem::path absolute_parent_path = cgroup_root / location.relative_parent_path;
    std::error_code error;
    const bool exists = std::filesystem::exists(absolute_parent_path, error);
    if (error) {
        warnings.push_back(make_warning(
            DockerContainerScanWarningCode::cgroup_directory_open_failed, absolute_parent_path,
            "Unable to check the Docker cgroup directory: " + error.message()));
        return;
    }
    if (!exists) {
        return;
    }

    found_layout = true;
    std::filesystem::directory_iterator iterator{absolute_parent_path, error};
    if (error) {
        warnings.push_back(make_warning(
            DockerContainerScanWarningCode::cgroup_directory_open_failed, absolute_parent_path,
            "Unable to open the Docker cgroup directory: " + error.message()));
        return;
    }

    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const std::filesystem::directory_entry entry = *iterator;
        const std::filesystem::path entry_path = entry.path();
        std::error_code entry_error;
        const bool is_directory = entry.is_directory(entry_error);
        if (entry_error) {
            warnings.push_back(make_warning(
                DockerContainerScanWarningCode::cgroup_directory_read_failed, entry_path,
                "Unable to inspect a Docker cgroup directory entry: " + entry_error.message()));
        } else if (is_directory) {
            const std::string name = entry.path().filename().string();
            if (location.layout == DockerCgroupLayout::systemd) {
                if (is_systemd_container_directory(name)) {
                    add_candidate(cgroup_root, location.relative_parent_path / name,
                                  location.layout, systemd_container_id_text(name), containers,
                                  skipped_container_count, warnings);
                }
            } else {
                add_candidate(cgroup_root, location.relative_parent_path / name, location.layout,
                              name, containers, skipped_container_count, warnings);
            }
        }

        iterator.increment(error);
        if (error) {
            warnings.push_back(make_warning(
                DockerContainerScanWarningCode::cgroup_directory_read_failed, absolute_parent_path,
                "Unable to continue reading the Docker cgroup directory: " + error.message()));
            break;
        }
    }
}

} // namespace

DockerContainerScanResult
scan_rootful_docker_containers_from_root(const std::filesystem::path &cgroup_root) {
    std::map<std::string, DiscoveredContainer> discovered;
    std::size_t skipped_container_count{};
    std::vector<DockerContainerScanWarning> warnings;
    bool found_layout{};

    for (const DockerCgroupLocation &location : kDockerCgroupLocations) {
        scan_location(cgroup_root, location, discovered, skipped_container_count, warnings,
                      found_layout);
    }

    if (!found_layout) {
        warnings.push_back(
            make_warning(DockerContainerScanWarningCode::docker_cgroup_layout_not_found,
                         cgroup_root, "No standard rootful Docker cgroup v2 layout was found."));
    }

    std::vector<DockerContainerMemorySnapshot> containers;
    containers.reserve(discovered.size());
    for (auto &[container_id, container] : discovered) {
        containers.push_back(std::move(container.snapshot));
    }
    return DockerContainerScanResult::create(std::move(containers), skipped_container_count,
                                             std::move(warnings));
}

} // namespace memoptima::linux::detail

namespace memoptima::linux {

DockerContainerScanResult scan_rootful_docker_containers(const CgroupV2Environment &environment) {
    return detail::scan_rootful_docker_containers_from_root(environment.cgroup_root());
}

} // namespace memoptima::linux
