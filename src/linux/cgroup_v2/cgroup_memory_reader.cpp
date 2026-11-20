#include <memoptima/linux/cgroup_memory_reader.hpp>

#include "linux/cgroup_v2/cgroup_memory_parser.hpp"
#include "linux/cgroup_v2/cgroup_memory_reader_impl.hpp"
#include "linux/cgroup_v2/psi_parser.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace memoptima::linux::detail {
namespace {

[[nodiscard]] CgroupMetricReadError
make_error(CgroupMetricReadErrorCode code, std::string_view source_name, std::string_view detail) {
    return {.code = code, .source_name = std::string{source_name}, .detail = std::string{detail}};
}

[[nodiscard]] bool is_safe_relative_path(const std::filesystem::path &path) noexcept {
    // The reader joins this path to the detected cgroup root, so traversal would escape that root.
    if (path.empty() || path.is_absolute()) {
        return false;
    }
    for (const auto &component : path) {
        if (component == "..") {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::expected<std::string, CgroupMetricReadError>
read_file(const std::filesystem::path &path, std::string_view source_name) {
    std::ifstream file{path};
    if (!file.is_open()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::file_open_failed, source_name,
                                          "Unable to open " + path.string() + '.'));
    }
    const std::string contents{std::istreambuf_iterator<char>{file},
                               std::istreambuf_iterator<char>{}};
    if (file.bad()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::file_read_failed, source_name,
                                          "Unable to read " + path.string() + '.'));
    }
    return contents;
}

[[nodiscard]] CgroupMetricReadError map_psi_error(const PsiParseError &error) {
    const CgroupMetricReadErrorCode code =
        error.code == PsiParseErrorCode::value_overflow ? CgroupMetricReadErrorCode::value_overflow
        : error.code == PsiParseErrorCode::value_out_of_range
            ? CgroupMetricReadErrorCode::value_out_of_range
        : error.code == PsiParseErrorCode::invalid_value ? CgroupMetricReadErrorCode::invalid_value
        : error.code == PsiParseErrorCode::missing_required_field ||
                error.code == PsiParseErrorCode::missing_pressure_type
            ? CgroupMetricReadErrorCode::missing_required_value
        : error.code == PsiParseErrorCode::duplicate_required_field ||
                error.code == PsiParseErrorCode::duplicate_pressure_type
            ? CgroupMetricReadErrorCode::duplicate_required_value
            : CgroupMetricReadErrorCode::invalid_format;
    return make_error(code, "memory.pressure", error.detail);
}

template <typename Value>
[[nodiscard]] CgroupMetric<Value> unavailable_metric(const CgroupMetricReadError &error) {
    return std::unexpected(error);
}

} // namespace

CgroupMemorySnapshot
read_cgroup_memory_snapshot_from_root(const std::filesystem::path &cgroup_root,
                                      const std::filesystem::path &relative_cgroup_path) {
    if (!is_safe_relative_path(relative_cgroup_path)) {
        const CgroupMetricReadError error =
            make_error(CgroupMetricReadErrorCode::invalid_cgroup_path, "cgroup path",
                       "The cgroup path must be relative and cannot contain '..'.");
        return {
            .current_memory = unavailable_metric<std::uint64_t>(error),
            .limits = {.high = unavailable_metric<std::optional<std::uint64_t>>(error),
                       .maximum = unavailable_metric<std::optional<std::uint64_t>>(error)},
            .events = {.high = unavailable_metric<std::uint64_t>(error),
                       .maximum = unavailable_metric<std::uint64_t>(error),
                       .oom = unavailable_metric<std::uint64_t>(error),
                       .oom_kill = unavailable_metric<std::uint64_t>(error)},
            .composition = {.anonymous = unavailable_metric<std::uint64_t>(error),
                            .file_backed = unavailable_metric<std::uint64_t>(error)},
            .current_swap = unavailable_metric<std::uint64_t>(error),
            .pressure = {.some = unavailable_metric<CgroupPressureLine>(error),
                         .full = unavailable_metric<CgroupPressureLine>(error)},
        };
    }

    const std::filesystem::path directory = cgroup_root / relative_cgroup_path;
    // Read each independent cgroup file so one unavailable metric does not discard usable evidence.
    const auto current_text = read_file(directory / "memory.current", "memory.current");
    const auto high_text = read_file(directory / "memory.high", "memory.high");
    const auto maximum_text = read_file(directory / "memory.max", "memory.max");
    const auto events_text = read_file(directory / "memory.events", "memory.events");
    const auto stat_text = read_file(directory / "memory.stat", "memory.stat");
    const auto swap_text = read_file(directory / "memory.swap.current", "memory.swap.current");
    const auto pressure_text = read_file(directory / "memory.pressure", "memory.pressure");

    const CgroupByteMetric current = current_text
                                         ? parse_cgroup_byte_value(*current_text, "memory.current")
                                         : unavailable_metric<std::uint64_t>(current_text.error());
    const CgroupMemoryLimitMetric high =
        high_text ? parse_cgroup_memory_limit(*high_text, "memory.high")
                  : unavailable_metric<std::optional<std::uint64_t>>(high_text.error());
    const CgroupMemoryLimitMetric maximum =
        maximum_text ? parse_cgroup_memory_limit(*maximum_text, "memory.max")
                     : unavailable_metric<std::optional<std::uint64_t>>(maximum_text.error());
    const CgroupByteMetric swap = swap_text
                                      ? parse_cgroup_byte_value(*swap_text, "memory.swap.current")
                                      : unavailable_metric<std::uint64_t>(swap_text.error());

    const auto events = events_text
                            ? parse_cgroup_memory_events(*events_text)
                            : std::expected<ParsedCgroupMemoryEvents, CgroupMetricReadError>{
                                  std::unexpected(events_text.error())};
    const auto composition =
        stat_text ? parse_cgroup_memory_stat(*stat_text)
                  : std::expected<ParsedCgroupMemoryComposition, CgroupMetricReadError>{
                        std::unexpected(stat_text.error())};

    const CgroupMemoryEvents event_metrics =
        events ? CgroupMemoryEvents{.high = CgroupEventMetric{events->high},
                                    .maximum = CgroupEventMetric{events->maximum},
                                    .oom = CgroupEventMetric{events->oom},
                                    .oom_kill = CgroupEventMetric{events->oom_kill}}
               : CgroupMemoryEvents{.high = unavailable_metric<std::uint64_t>(events.error()),
                                    .maximum = unavailable_metric<std::uint64_t>(events.error()),
                                    .oom = unavailable_metric<std::uint64_t>(events.error()),
                                    .oom_kill = unavailable_metric<std::uint64_t>(events.error())};
    const CgroupMemoryComposition composition_metrics =
        composition
            ? CgroupMemoryComposition{.anonymous = CgroupByteMetric{composition->anonymous},
                                      .file_backed = CgroupByteMetric{composition->file_backed}}
            : CgroupMemoryComposition{
                  .anonymous = unavailable_metric<std::uint64_t>(composition.error()),
                  .file_backed = unavailable_metric<std::uint64_t>(composition.error())};
    CgroupMemoryPressure pressure_metrics;
    if (!pressure_text.has_value()) {
        pressure_metrics = {
            .some = unavailable_metric<CgroupPressureLine>(pressure_text.error()),
            .full = unavailable_metric<CgroupPressureLine>(pressure_text.error()),
        };
    } else {
        const auto pressure = parse_memory_pressure(*pressure_text);
        if (!pressure.has_value()) {
            // PSI is one coupled record: a malformed record cannot safely provide either pressure
            // line.
            const CgroupMetricReadError error = map_psi_error(pressure.error());
            pressure_metrics = {
                .some = unavailable_metric<CgroupPressureLine>(error),
                .full = unavailable_metric<CgroupPressureLine>(error),
            };
        } else {
            pressure_metrics = {
                .some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{
                    .average_10_seconds = pressure->some.average_10_seconds,
                    .average_60_seconds = pressure->some.average_60_seconds,
                    .average_300_seconds = pressure->some.average_300_seconds,
                    .total_microseconds = pressure->some.total_microseconds}},
                .full = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{
                    .average_10_seconds = pressure->full.average_10_seconds,
                    .average_60_seconds = pressure->full.average_60_seconds,
                    .average_300_seconds = pressure->full.average_300_seconds,
                    .total_microseconds = pressure->full.total_microseconds}},
            };
        }
    }
    return {
        .current_memory = current,
        .limits = {.high = high, .maximum = maximum},
        .events = event_metrics,
        .composition = composition_metrics,
        .current_swap = swap,
        .pressure = pressure_metrics,
    };
}

} // namespace memoptima::linux::detail

namespace memoptima::linux {

CgroupMemorySnapshot
read_cgroup_memory_snapshot(const CgroupV2Environment &environment,
                            const std::filesystem::path &relative_cgroup_path) {
    return detail::read_cgroup_memory_snapshot_from_root(environment.cgroup_root(),
                                                         relative_cgroup_path);
}

} // namespace memoptima::linux
