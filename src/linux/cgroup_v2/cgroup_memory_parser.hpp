#pragma once

#include <memoptima/cgroup_memory_snapshot.hpp>

#include <expected>
#include <string_view>

namespace memoptima::linux::detail {

struct ParsedCgroupMemoryEvents {
    std::uint64_t high;
    std::uint64_t maximum;
    std::uint64_t oom;
    std::uint64_t oom_kill;
};

struct ParsedCgroupMemoryComposition {
    std::uint64_t anonymous;
    std::uint64_t file_backed;
};

[[nodiscard]] std::expected<std::uint64_t, CgroupMetricReadError>
parse_cgroup_byte_value(std::string_view text, std::string_view source_name);

[[nodiscard]] std::expected<std::optional<std::uint64_t>, CgroupMetricReadError>
parse_cgroup_memory_limit(std::string_view text, std::string_view source_name);

[[nodiscard]] std::expected<ParsedCgroupMemoryEvents, CgroupMetricReadError>
parse_cgroup_memory_events(std::string_view text);

[[nodiscard]] std::expected<ParsedCgroupMemoryComposition, CgroupMetricReadError>
parse_cgroup_memory_stat(std::string_view text);

} // namespace memoptima::linux::detail
