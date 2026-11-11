#pragma once

#include <memoptima/cgroup_memory_snapshot.hpp>

#include <filesystem>

namespace memoptima::linux::detail {

[[nodiscard]] CgroupMemorySnapshot
read_cgroup_memory_snapshot_from_root(const std::filesystem::path &cgroup_root,
                                      const std::filesystem::path &relative_cgroup_path);

} // namespace memoptima::linux::detail
