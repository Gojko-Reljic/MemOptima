#include "cli/containers_formatter.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr std::uint64_t kBytesPerUnit = 1024;
constexpr std::array<std::string_view, 5> kUnitNames{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr int kContainerIdColumnWidth = 66;
constexpr int kCurrentMemoryColumnWidth = 14;
constexpr int kMemoryHighColumnWidth = 14;
constexpr int kMemoryMaxColumnWidth = 14;
constexpr int kOomKillsColumnWidth = 12;

[[nodiscard]] std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0) {
        return "0 B";
    }

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};
    while (display_value >= static_cast<double>(kBytesPerUnit) &&
           unit_index + 1 < kUnitNames.size()) {
        display_value /= static_cast<double>(kBytesPerUnit);
        ++unit_index;
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << display_value << ' ' << kUnitNames[unit_index];
    return output.str();
}

[[nodiscard]] std::string format_byte_metric(const CgroupByteMetric &metric) {
    return metric ? format_bytes(*metric) : "N/A";
}

[[nodiscard]] std::string format_memory_limit(const CgroupMemoryLimitMetric &limit) {
    if (!limit.has_value()) {
        return "N/A";
    }
    return limit->has_value() ? format_bytes(**limit) : "max";
}

[[nodiscard]] std::string format_event_metric(const CgroupEventMetric &metric) {
    return metric ? std::to_string(*metric) : "N/A";
}

[[nodiscard]] std::string format_psi_some_average_10(const CgroupMemoryPressure &pressure) {
    if (!pressure.some.has_value()) {
        return "N/A";
    }

    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << pressure.some->average_10_seconds << '%';
    return output.str();
}

} // namespace

std::string format_docker_container_scan_result(const DockerContainerScanResult &result) {
    if (result.containers().empty()) {
        return "No supported rootful Docker containers found.\n";
    }

    std::ostringstream output;
    output << "Docker container memory\n\n"
           << std::left << std::setw(kContainerIdColumnWidth) << "CONTAINER ID"
           << std::setw(kCurrentMemoryColumnWidth) << "CURRENT" << std::setw(kMemoryHighColumnWidth)
           << "MEMORY.HIGH" << std::setw(kMemoryMaxColumnWidth) << "MEMORY.MAX"
           << std::setw(kOomKillsColumnWidth) << "OOM KILLS"
           << "PSI SOME AVG10\n";

    for (const DockerContainerMemorySnapshot &container : result.containers()) {
        const CgroupMemorySnapshot &memory = container.memory();
        output << std::left << std::setw(kContainerIdColumnWidth)
               << container.container_id().value() << std::setw(kCurrentMemoryColumnWidth)
               << format_byte_metric(memory.current_memory()) << std::setw(kMemoryHighColumnWidth)
               << format_memory_limit(memory.limits().high) << std::setw(kMemoryMaxColumnWidth)
               << format_memory_limit(memory.limits().maximum) << std::setw(kOomKillsColumnWidth)
               << format_event_metric(memory.events().oom_kill)
               << format_psi_some_average_10(memory.pressure()) << '\n';
    }
    return output.str();
}

std::string format_docker_container_scan_warnings(const DockerContainerScanResult &result) {
    std::ostringstream output;
    for (const DockerContainerScanWarning &warning : result.warnings()) {
        output << "Warning: " << warning.detail << " Path: " << warning.cgroup_path.string()
               << '\n';
    }
    return output.str();
}

} // namespace memoptima::cli
