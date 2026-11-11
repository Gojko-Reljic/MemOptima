#pragma once

#include <memoptima/cgroup_memory_snapshot.hpp>
#include <memoptima/linux/cgroup_v2_capability_checker.hpp>

#include <filesystem>

namespace memoptima::linux {

[[nodiscard]] CgroupMemorySnapshot
read_cgroup_memory_snapshot(const CgroupV2Environment &environment,
                            const std::filesystem::path &relative_cgroup_path);

} // namespace memoptima::linux
