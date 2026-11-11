#include "linux/cgroup_v2/cgroup_memory_parser.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace memoptima::linux::detail {
namespace {

[[nodiscard]] CgroupMetricReadError
make_error(CgroupMetricReadErrorCode code, std::string_view source_name, std::string_view detail) {
    return {.code = code, .source_name = std::string{source_name}, .detail = std::string{detail}};
}

[[nodiscard]] std::string_view trim_ascii_whitespace(std::string_view text) noexcept {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' ||
                             text.front() == '\n')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
                             text.back() == '\n')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] std::expected<std::uint64_t, CgroupMetricReadError>
parse_unsigned_value(std::string_view text, std::string_view source_name) {
    const std::string_view value_text = trim_ascii_whitespace(text);
    if (value_text.empty()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::invalid_value, source_name,
                                          "Expected an unsigned integer value."));
    }

    std::uint64_t value{};
    const auto [parse_end, parse_error] =
        std::from_chars(value_text.data(), value_text.data() + value_text.size(), value);
    if (parse_error == std::errc::result_out_of_range) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::value_overflow, source_name,
                                          "The value does not fit in an unsigned 64-bit integer."));
    }
    if (parse_error != std::errc{} || parse_end != value_text.data() + value_text.size()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::invalid_value, source_name,
                                          "Expected an unsigned integer value."));
    }
    return value;
}

template <typename Value>
[[nodiscard]] std::expected<std::monostate, CgroupMetricReadError>
store_required_value(std::optional<Value> &destination, Value value, std::string_view source_name,
                     std::string_view field_name) {
    if (destination.has_value()) {
        return std::unexpected(make_error(
            CgroupMetricReadErrorCode::duplicate_required_value, source_name,
            std::string{"The field "} + std::string{field_name} + " appears more than once."));
    }
    destination = value;
    return std::monostate{};
}

} // namespace

std::expected<std::uint64_t, CgroupMetricReadError>
parse_cgroup_byte_value(std::string_view text, std::string_view source_name) {
    return parse_unsigned_value(text, source_name);
}

std::expected<std::optional<std::uint64_t>, CgroupMetricReadError>
parse_cgroup_memory_limit(std::string_view text, std::string_view source_name) {
    const std::string_view value_text = trim_ascii_whitespace(text);
    if (value_text == "max") {
        return std::optional<std::uint64_t>{};
    }
    const auto value = parse_unsigned_value(value_text, source_name);
    if (!value.has_value()) {
        return std::unexpected(value.error());
    }
    return std::optional<std::uint64_t>{*value};
}

std::expected<ParsedCgroupMemoryEvents, CgroupMetricReadError>
parse_cgroup_memory_events(std::string_view text) {
    constexpr std::string_view source_name{"memory.events"};
    std::optional<std::uint64_t> high;
    std::optional<std::uint64_t> maximum;
    std::optional<std::uint64_t> oom;
    std::optional<std::uint64_t> oom_kill;
    std::size_t line_start{};

    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line =
            trim_ascii_whitespace(text.substr(line_start, line_end - line_start));
        if (!line.empty()) {
            const std::size_t separator = line.find_first_of(" \t");
            if (separator == std::string_view::npos) {
                return std::unexpected(
                    make_error(CgroupMetricReadErrorCode::invalid_format, source_name,
                               "Expected an event name and an unsigned integer value."));
            }
            const std::string_view name = line.substr(0, separator);
            const auto value = parse_unsigned_value(
                trim_ascii_whitespace(line.substr(separator + 1)), source_name);
            if (!value.has_value()) {
                return std::unexpected(value.error());
            }
            std::expected<std::monostate, CgroupMetricReadError> stored{std::monostate{}};
            if (name == "high") {
                stored = store_required_value(high, *value, source_name, name);
            } else if (name == "max") {
                stored = store_required_value(maximum, *value, source_name, name);
            } else if (name == "oom") {
                stored = store_required_value(oom, *value, source_name, name);
            } else if (name == "oom_kill") {
                stored = store_required_value(oom_kill, *value, source_name, name);
            }
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    if (!high.has_value() || !maximum.has_value() || !oom.has_value() || !oom_kill.has_value()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::missing_required_value,
                                          source_name,
                                          "The file is missing a required event counter."));
    }
    return ParsedCgroupMemoryEvents{
        .high = *high, .maximum = *maximum, .oom = *oom, .oom_kill = *oom_kill};
}

std::expected<ParsedCgroupMemoryComposition, CgroupMetricReadError>
parse_cgroup_memory_stat(std::string_view text) {
    constexpr std::string_view source_name{"memory.stat"};
    std::optional<std::uint64_t> anonymous;
    std::optional<std::uint64_t> file_backed;
    std::size_t line_start{};
    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line =
            trim_ascii_whitespace(text.substr(line_start, line_end - line_start));
        if (!line.empty()) {
            const std::size_t separator = line.find_first_of(" \t");
            if (separator == std::string_view::npos) {
                return std::unexpected(
                    make_error(CgroupMetricReadErrorCode::invalid_format, source_name,
                               "Expected a statistic name and an unsigned integer value."));
            }
            const std::string_view name = line.substr(0, separator);
            if (name == "anon" || name == "file") {
                const auto value = parse_unsigned_value(
                    trim_ascii_whitespace(line.substr(separator + 1)), source_name);
                if (!value.has_value()) {
                    return std::unexpected(value.error());
                }
                const auto stored =
                    name == "anon" ? store_required_value(anonymous, *value, source_name, name)
                                   : store_required_value(file_backed, *value, source_name, name);
                if (!stored.has_value()) {
                    return std::unexpected(stored.error());
                }
            }
        }
        if (line_end == std::string_view::npos) {
            break;
        }
        line_start = line_end + 1;
    }
    if (!anonymous.has_value() || !file_backed.has_value()) {
        return std::unexpected(make_error(CgroupMetricReadErrorCode::missing_required_value,
                                          source_name, "The file is missing anon or file."));
    }
    return ParsedCgroupMemoryComposition{.anonymous = *anonymous, .file_backed = *file_backed};
}

} // namespace memoptima::linux::detail
