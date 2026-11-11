#include <memoptima/cgroup_memory_snapshot.hpp>

namespace memoptima {

CgroupMemorySnapshot
CgroupMemorySnapshot::create(CgroupByteMetric current_memory, CgroupMemoryLimits limits,
                             CgroupMemoryEvents events, CgroupMemoryComposition composition,
                             CgroupByteMetric current_swap, CgroupMemoryPressure pressure) {
    return CgroupMemorySnapshot{std::move(current_memory), std::move(limits),
                                std::move(events),         std::move(composition),
                                std::move(current_swap),   std::move(pressure)};
}

const CgroupByteMetric &CgroupMemorySnapshot::current_memory() const noexcept {
    return current_memory_;
}

const CgroupMemoryLimits &CgroupMemorySnapshot::limits() const noexcept { return limits_; }

const CgroupMemoryEvents &CgroupMemorySnapshot::events() const noexcept { return events_; }

const CgroupMemoryComposition &CgroupMemorySnapshot::composition() const noexcept {
    return composition_;
}

const CgroupByteMetric &CgroupMemorySnapshot::current_swap() const noexcept {
    return current_swap_;
}

const CgroupMemoryPressure &CgroupMemorySnapshot::pressure() const noexcept { return pressure_; }

} // namespace memoptima
