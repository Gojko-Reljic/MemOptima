#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace memoptima {

enum class CgroupMetricReadErrorCode {
    invalid_cgroup_path,
    file_open_failed,
    file_read_failed,
    invalid_format,
    missing_required_value,
    duplicate_required_value,
    invalid_value,
    value_out_of_range,
    value_overflow,
};

struct CgroupMetricReadError {
    CgroupMetricReadErrorCode code;
    std::string source_name;
    std::string detail;
};

// Every metric is independent: one unreadable cgroup file must not discard the useful metrics.
template <typename Value> using CgroupMetric = std::expected<Value, CgroupMetricReadError>;

using CgroupUnsignedMetric = CgroupMetric<std::uint64_t>;
using CgroupByteMetric = CgroupUnsignedMetric;
using CgroupEventMetric = CgroupUnsignedMetric;
using CgroupMemoryLimitMetric = CgroupMetric<std::optional<std::uint64_t>>;

struct CgroupMemoryLimits {
    CgroupMemoryLimitMetric high;
    CgroupMemoryLimitMetric maximum;
};

struct CgroupMemoryEvents {
    CgroupEventMetric high;
    CgroupEventMetric maximum;
    CgroupEventMetric oom;
    CgroupEventMetric oom_kill;
};

struct CgroupMemoryComposition {
    CgroupByteMetric anonymous;
    CgroupByteMetric file_backed;
};

struct CgroupPressureLine {
    double average_10_seconds;
    double average_60_seconds;
    double average_300_seconds;
    std::uint64_t total_microseconds;
};

struct CgroupMemoryPressure {
    CgroupMetric<CgroupPressureLine> some;
    CgroupMetric<CgroupPressureLine> full;
};

// Raw cgroup v2 evidence. Analysis deliberately consumes partial snapshots when possible.
struct CgroupMemorySnapshot {
    CgroupByteMetric current_memory;
    CgroupMemoryLimits limits;
    CgroupMemoryEvents events;
    CgroupMemoryComposition composition;
    CgroupByteMetric current_swap;
    CgroupMemoryPressure pressure;
};

} // namespace memoptima
