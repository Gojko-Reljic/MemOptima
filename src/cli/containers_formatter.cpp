#include "cli/containers_formatter.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>

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

[[nodiscard]] std::string format_pressure_average(double average) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(2) << average << '%';
    return output.str();
}

[[nodiscard]] std::string format_pressure_total(std::uint64_t microseconds) {
    if (microseconds < 1000) {
        return std::to_string(microseconds) + " µs";
    }

    const double milliseconds = static_cast<double>(microseconds) / 1000.0;
    std::ostringstream output;
    output << std::fixed << std::setprecision(2);
    if (milliseconds < 1000.0) {
        output << milliseconds << " ms";
    } else {
        output << milliseconds / 1000.0 << " s";
    }
    return output.str();
}

[[nodiscard]] std::string format_pressure_line(const CgroupMetric<CgroupPressureLine> &line) {
    if (!line.has_value()) {
        return "N/A";
    }

    std::ostringstream output;
    output << "avg10=" << format_pressure_average(line->average_10_seconds)
           << ", avg60=" << format_pressure_average(line->average_60_seconds)
           << ", avg300=" << format_pressure_average(line->average_300_seconds)
           << ", total=" << format_pressure_total(line->total_microseconds);
    return output.str();
}

using UnavailableDataKey = std::tuple<CgroupMetricReadErrorCode, std::string, std::string>;

void add_unavailable_metric(std::set<UnavailableDataKey> &unavailable_data,
                            const CgroupMetricReadError &error) {
    unavailable_data.emplace(error.code, error.source_name, error.detail);
}

template <typename Value>
void add_unavailable_metric(std::set<UnavailableDataKey> &unavailable_data,
                            const CgroupMetric<Value> &metric) {
    if (!metric.has_value()) {
        add_unavailable_metric(unavailable_data, metric.error());
    }
}

[[nodiscard]] std::set<UnavailableDataKey>
collect_unavailable_data(const CgroupMemorySnapshot &memory) {
    std::set<UnavailableDataKey> unavailable_data;
    add_unavailable_metric(unavailable_data, memory.current_memory());
    add_unavailable_metric(unavailable_data, memory.limits().high);
    add_unavailable_metric(unavailable_data, memory.limits().maximum);
    add_unavailable_metric(unavailable_data, memory.events().high);
    add_unavailable_metric(unavailable_data, memory.events().maximum);
    add_unavailable_metric(unavailable_data, memory.events().oom);
    add_unavailable_metric(unavailable_data, memory.events().oom_kill);
    add_unavailable_metric(unavailable_data, memory.composition().anonymous);
    add_unavailable_metric(unavailable_data, memory.composition().file_backed);
    add_unavailable_metric(unavailable_data, memory.current_swap());
    add_unavailable_metric(unavailable_data, memory.pressure().some);
    add_unavailable_metric(unavailable_data, memory.pressure().full);
    return unavailable_data;
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
    return format_docker_container_scan_warnings(result.warnings());
}

std::string
format_docker_container_scan_warnings(const std::vector<DockerContainerScanWarning> &warnings) {
    std::ostringstream output;
    for (const DockerContainerScanWarning &warning : warnings) {
        output << "Warning: " << warning.detail << " Path: " << warning.cgroup_path.string()
               << '\n';
    }
    return output.str();
}

std::string format_docker_container_detail(const DockerContainerMemorySnapshot &container) {
    const CgroupMemorySnapshot &memory = container.memory();
    std::ostringstream output;
    output << "Container\n"
           << "  ID:                  " << container.container_id().value() << '\n'
           << "  Cgroup path:         " << container.cgroup_path().string() << "\n\n"
           << "Memory\n"
           << "  Current:             " << format_byte_metric(memory.current_memory()) << '\n'
           << "  High:                " << format_memory_limit(memory.limits().high) << '\n'
           << "  Max:                 " << format_memory_limit(memory.limits().maximum) << "\n\n"
           << "Events\n"
           << "  High:                " << format_event_metric(memory.events().high) << '\n'
           << "  Max:                 " << format_event_metric(memory.events().maximum) << '\n'
           << "  OOM:                 " << format_event_metric(memory.events().oom) << '\n'
           << "  OOM kills:           " << format_event_metric(memory.events().oom_kill) << "\n\n"
           << "Composition\n"
           << "  Anonymous:           " << format_byte_metric(memory.composition().anonymous)
           << '\n'
           << "  File-backed:         " << format_byte_metric(memory.composition().file_backed)
           << "\n\n"
           << "Swap\n"
           << "  Current:             " << format_byte_metric(memory.current_swap()) << "\n\n"
           << "Memory pressure\n"
           << "  Some:                " << format_pressure_line(memory.pressure().some) << '\n'
           << "  Full:                " << format_pressure_line(memory.pressure().full) << '\n';

    const std::set<UnavailableDataKey> unavailable_data = collect_unavailable_data(memory);
    if (!unavailable_data.empty()) {
        output << "\nUnavailable data\n";
        for (const UnavailableDataKey &error : unavailable_data) {
            output << "  " << std::get<1>(error) << ": " << std::get<2>(error) << '\n';
        }
    }
    return output.str();
}

} // namespace memoptima::cli
