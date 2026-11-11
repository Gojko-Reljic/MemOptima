#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <utility>

namespace memoptima {

enum class CgroupMetricReadErrorCode {
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

class CgroupMemorySnapshot {
  public:
    [[nodiscard]] static CgroupMemorySnapshot
    create(CgroupByteMetric current_memory, CgroupMemoryLimits limits, CgroupMemoryEvents events,
           CgroupMemoryComposition composition, CgroupByteMetric current_swap,
           CgroupMemoryPressure pressure);

    [[nodiscard]] const CgroupByteMetric &current_memory() const noexcept;
    [[nodiscard]] const CgroupMemoryLimits &limits() const noexcept;
    [[nodiscard]] const CgroupMemoryEvents &events() const noexcept;
    [[nodiscard]] const CgroupMemoryComposition &composition() const noexcept;
    [[nodiscard]] const CgroupByteMetric &current_swap() const noexcept;
    [[nodiscard]] const CgroupMemoryPressure &pressure() const noexcept;

  private:
    CgroupMemorySnapshot(CgroupByteMetric current_memory, CgroupMemoryLimits limits,
                         CgroupMemoryEvents events, CgroupMemoryComposition composition,
                         CgroupByteMetric current_swap, CgroupMemoryPressure pressure)
        : current_memory_{std::move(current_memory)}, limits_{std::move(limits)},
          events_{std::move(events)}, composition_{std::move(composition)},
          current_swap_{std::move(current_swap)}, pressure_{std::move(pressure)} {}

    CgroupByteMetric current_memory_;
    CgroupMemoryLimits limits_;
    CgroupMemoryEvents events_;
    CgroupMemoryComposition composition_;
    CgroupByteMetric current_swap_;
    CgroupMemoryPressure pressure_;
};

} // namespace memoptima
