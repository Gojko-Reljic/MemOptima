#include "linux/meminfo_parser.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace memoptima::linux::detail {
namespace {

// /proc/meminfo labels binary kilobytes as kB; MemOptima converts them to bytes internally.
constexpr std::uint64_t kBytesPerKilobyte = 1024;

struct MetricValue {
    std::uint64_t bytes;
    std::size_t line_number;
};

struct ParsedMetrics {
    std::optional<MetricValue> total_memory;
    std::optional<MetricValue> available_memory;
    std::optional<MetricValue> total_swap;
    std::optional<MetricValue> free_swap;
};

[[nodiscard]] bool is_ascii_whitespace(char character) noexcept {
    return character == ' ' || character == '\t' || character == '\r';
}

[[nodiscard]] std::string_view trim_ascii_whitespace(std::string_view text) noexcept {
    while (!text.empty() && is_ascii_whitespace(text.front())) {
        text.remove_prefix(1);
    }

    while (!text.empty() && is_ascii_whitespace(text.back())) {
        text.remove_suffix(1);
    }

    return text;
}

[[nodiscard]] MemoryReadError make_error(MemoryReadErrorCode code, std::string_view metric_name,
                                         std::size_t line_number, std::string_view detail) {
    return {
        .code = code,
        .metric_name = std::string{metric_name},
        .line_number = line_number,
        .detail = std::string{detail},
    };
}

[[nodiscard]] std::expected<std::uint64_t, MemoryReadError>
parse_value_in_bytes(std::string_view value_and_unit, std::string_view metric_name,
                     std::size_t line_number) {
    const std::string_view trimmed_value = trim_ascii_whitespace(value_and_unit);
    const std::size_t value_end = trimmed_value.find_first_of(" \t\r");
    const std::string_view value_text = trimmed_value.substr(0, value_end);

    if (value_text.empty()) {
        return std::unexpected(make_error(MemoryReadErrorCode::invalid_value, metric_name,
                                          line_number, "Expected an unsigned integer value."));
    }

    std::uint64_t value_in_kilobytes{};
    const auto [parse_end, parse_error] = std::from_chars(
        value_text.data(), value_text.data() + value_text.size(), value_in_kilobytes);

    if (parse_error == std::errc::result_out_of_range) {
        return std::unexpected(make_error(MemoryReadErrorCode::value_overflow, metric_name,
                                          line_number,
                                          "The value does not fit in an unsigned 64-bit integer."));
    }

    if (parse_error != std::errc{} || parse_end != value_text.data() + value_text.size()) {
        return std::unexpected(make_error(MemoryReadErrorCode::invalid_value, metric_name,
                                          line_number, "Expected an unsigned integer value."));
    }

    const std::string_view unit = trim_ascii_whitespace(trimmed_value.substr(value_end));
    if (unit != "kB") {
        return std::unexpected(make_error(MemoryReadErrorCode::unexpected_unit, metric_name,
                                          line_number, "Expected the unit kB."));
    }

    if (value_in_kilobytes > std::numeric_limits<std::uint64_t>::max() / kBytesPerKilobyte) {
        return std::unexpected(
            make_error(MemoryReadErrorCode::value_overflow, metric_name, line_number,
                       "The value cannot be converted from kB to bytes without overflow."));
    }

    return value_in_kilobytes * kBytesPerKilobyte;
}

[[nodiscard]] std::expected<std::monostate, MemoryReadError>
store_metric(std::optional<MetricValue> &destination, std::string_view value_and_unit,
             std::string_view metric_name, std::size_t line_number) {
    if (destination.has_value()) {
        return std::unexpected(make_error(MemoryReadErrorCode::duplicate_required_metric,
                                          metric_name, line_number,
                                          "The metric appears more than once."));
    }

    const auto value_in_bytes = parse_value_in_bytes(value_and_unit, metric_name, line_number);
    if (!value_in_bytes.has_value()) {
        return std::unexpected(value_in_bytes.error());
    }

    destination = MetricValue{
        .bytes = *value_in_bytes,
        .line_number = line_number,
    };
    return std::monostate{};
}

[[nodiscard]] std::expected<MetricValue, MemoryReadError>
require_metric(const std::optional<MetricValue> &metric, std::string_view metric_name) {
    if (!metric.has_value()) {
        return std::unexpected(make_error(MemoryReadErrorCode::missing_required_metric, metric_name,
                                          0, "The required metric is missing."));
    }

    return *metric;
}

[[nodiscard]] std::expected<MemorySnapshot, MemoryReadError>
validate_metrics(const ParsedMetrics &metrics) {
    const auto total_memory = require_metric(metrics.total_memory, "MemTotal");
    if (!total_memory.has_value()) {
        return std::unexpected(total_memory.error());
    }

    const auto available_memory = require_metric(metrics.available_memory, "MemAvailable");
    if (!available_memory.has_value()) {
        return std::unexpected(available_memory.error());
    }

    const auto total_swap = require_metric(metrics.total_swap, "SwapTotal");
    if (!total_swap.has_value()) {
        return std::unexpected(total_swap.error());
    }

    const auto free_swap = require_metric(metrics.free_swap, "SwapFree");
    if (!free_swap.has_value()) {
        return std::unexpected(free_swap.error());
    }

    const auto snapshot = MemorySnapshot::create(total_memory->bytes, available_memory->bytes,
                                                 total_swap->bytes, free_swap->bytes);

    if (snapshot.has_value()) {
        return *snapshot;
    }

    switch (snapshot.error().code) {
    case SnapshotValidationErrorCode::total_memory_must_be_positive:
        return std::unexpected(make_error(MemoryReadErrorCode::inconsistent_values, "MemTotal",
                                          total_memory->line_number,
                                          "MemTotal must be greater than zero."));
    case SnapshotValidationErrorCode::available_memory_exceeds_total:
        return std::unexpected(make_error(MemoryReadErrorCode::inconsistent_values, "MemAvailable",
                                          available_memory->line_number,
                                          "MemAvailable cannot exceed MemTotal."));
    case SnapshotValidationErrorCode::free_swap_exceeds_total:
        return std::unexpected(make_error(MemoryReadErrorCode::inconsistent_values, "SwapFree",
                                          free_swap->line_number,
                                          "SwapFree cannot exceed SwapTotal."));
    }

    return std::unexpected(make_error(MemoryReadErrorCode::inconsistent_values, {}, 0,
                                      "Memory snapshot validation failed."));
}

} // namespace

std::expected<MemorySnapshot, MemoryReadError> parse_memory_info(std::string_view text) {
    ParsedMetrics metrics;
    std::size_t line_number{};
    std::size_t line_start{};

    while (line_start < text.size()) {
        ++line_number;
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line = text.substr(line_start, line_end - line_start);
        const std::size_t separator = line.find(':');

        if (separator != std::string_view::npos) {
            const std::string_view metric_name = line.substr(0, separator);
            const std::string_view value_and_unit = line.substr(separator + 1);

            std::expected<std::monostate, MemoryReadError> stored_metric{std::monostate{}};

            // Ignore unrelated kernel fields while strictly validating the four metrics we require.
            if (metric_name == "MemTotal") {
                stored_metric =
                    store_metric(metrics.total_memory, value_and_unit, metric_name, line_number);
            } else if (metric_name == "MemAvailable") {
                stored_metric = store_metric(metrics.available_memory, value_and_unit, metric_name,
                                             line_number);
            } else if (metric_name == "SwapTotal") {
                stored_metric =
                    store_metric(metrics.total_swap, value_and_unit, metric_name, line_number);
            } else if (metric_name == "SwapFree") {
                stored_metric =
                    store_metric(metrics.free_swap, value_and_unit, metric_name, line_number);
            }

            if (!stored_metric.has_value()) {
                return std::unexpected(stored_metric.error());
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }

        line_start = line_end + 1;
    }

    return validate_metrics(metrics);
}

} // namespace memoptima::linux::detail
