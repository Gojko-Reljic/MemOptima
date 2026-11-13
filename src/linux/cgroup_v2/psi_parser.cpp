#include "linux/cgroup_v2/psi_parser.hpp"
#include "linux/text_utils.hpp"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace memoptima::linux::detail {
namespace {

struct ParsedPressureLine {
    std::optional<double> average_10_seconds;
    std::optional<double> average_60_seconds;
    std::optional<double> average_300_seconds;
    std::optional<std::uint64_t> total_microseconds;
};

[[nodiscard]] PsiParseError make_error(PsiParseErrorCode code, std::string_view detail) {
    return {
        .code = code,
        .detail = std::string{detail},
    };
}

[[nodiscard]] std::vector<std::string_view> split_ascii_whitespace(std::string_view text) {
    std::vector<std::string_view> fields;
    std::size_t position{};

    while (position < text.size()) {
        while (position < text.size() && is_ascii_whitespace(text[position])) {
            ++position;
        }

        const std::size_t field_start = position;
        while (position < text.size() && !is_ascii_whitespace(text[position])) {
            ++position;
        }

        if (field_start != position) {
            fields.push_back(text.substr(field_start, position - field_start));
        }
    }

    return fields;
}

[[nodiscard]] std::expected<double, PsiParseError> parse_percentage(std::string_view value_text) {
    double value{};
    const auto [parse_end, parse_error] =
        std::from_chars(value_text.data(), value_text.data() + value_text.size(), value);

    if (parse_error != std::errc{} || parse_end != value_text.data() + value_text.size() ||
        !std::isfinite(value)) {
        return std::unexpected(
            make_error(PsiParseErrorCode::invalid_value, "Expected a finite decimal percentage."));
    }

    if (value < 0.0 || value > 100.0) {
        return std::unexpected(make_error(PsiParseErrorCode::value_out_of_range,
                                          "Percentage must be between 0 and 100."));
    }

    return value;
}

[[nodiscard]] std::expected<std::uint64_t, PsiParseError>
parse_total_microseconds(std::string_view value_text) {
    std::uint64_t value{};
    const auto [parse_end, parse_error] =
        std::from_chars(value_text.data(), value_text.data() + value_text.size(), value);

    if (parse_error == std::errc::result_out_of_range) {
        return std::unexpected(make_error(PsiParseErrorCode::value_overflow,
                                          "Total does not fit in an unsigned 64-bit integer."));
    }

    if (parse_error != std::errc{} || parse_end != value_text.data() + value_text.size()) {
        return std::unexpected(
            make_error(PsiParseErrorCode::invalid_value, "Expected an unsigned integer total."));
    }

    return value;
}

template <typename Value>
[[nodiscard]] std::expected<std::monostate, PsiParseError>
store_once(std::optional<Value> &destination, Value value, std::string_view field_name) {
    if (destination.has_value()) {
        return std::unexpected(make_error(PsiParseErrorCode::duplicate_required_field,
                                          std::string{"The field "} + std::string{field_name} +
                                              " appears more than once."));
    }

    destination = value;
    return std::monostate{};
}

[[nodiscard]] std::expected<PsiPressureLine, PsiParseError>
parse_pressure_line(const std::vector<std::string_view> &fields) {
    if (fields.size() != 5) {
        return std::unexpected(make_error(PsiParseErrorCode::invalid_format,
                                          "Pressure line must contain a type and four fields."));
    }

    ParsedPressureLine parsed;
    for (std::size_t index = 1; index < fields.size(); ++index) {
        const std::string_view field = fields[index];
        const std::size_t separator = field.find('=');
        if (separator == std::string_view::npos || separator == 0 ||
            separator + 1 >= field.size()) {
            return std::unexpected(make_error(PsiParseErrorCode::invalid_format,
                                              "Pressure field must use key=value format."));
        }

        const std::string_view key = field.substr(0, separator);
        const std::string_view value = field.substr(separator + 1);

        if (key == "avg10") {
            const auto parsed_value = parse_percentage(value);
            if (!parsed_value.has_value()) {
                return std::unexpected(parsed_value.error());
            }
            const auto stored = store_once(parsed.average_10_seconds, *parsed_value, key);
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
        } else if (key == "avg60") {
            const auto parsed_value = parse_percentage(value);
            if (!parsed_value.has_value()) {
                return std::unexpected(parsed_value.error());
            }
            const auto stored = store_once(parsed.average_60_seconds, *parsed_value, key);
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
        } else if (key == "avg300") {
            const auto parsed_value = parse_percentage(value);
            if (!parsed_value.has_value()) {
                return std::unexpected(parsed_value.error());
            }
            const auto stored = store_once(parsed.average_300_seconds, *parsed_value, key);
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
        } else if (key == "total") {
            const auto parsed_value = parse_total_microseconds(value);
            if (!parsed_value.has_value()) {
                return std::unexpected(parsed_value.error());
            }
            const auto stored = store_once(parsed.total_microseconds, *parsed_value, key);
            if (!stored.has_value()) {
                return std::unexpected(stored.error());
            }
        } else {
            return std::unexpected(make_error(PsiParseErrorCode::invalid_format,
                                              "Pressure line contains an unknown field."));
        }
    }

    if (!parsed.average_10_seconds.has_value() || !parsed.average_60_seconds.has_value() ||
        !parsed.average_300_seconds.has_value() || !parsed.total_microseconds.has_value()) {
        return std::unexpected(make_error(PsiParseErrorCode::missing_required_field,
                                          "Pressure line is missing a required field."));
    }

    return PsiPressureLine{
        .average_10_seconds = *parsed.average_10_seconds,
        .average_60_seconds = *parsed.average_60_seconds,
        .average_300_seconds = *parsed.average_300_seconds,
        .total_microseconds = *parsed.total_microseconds,
    };
}

} // namespace

std::expected<PsiMemoryPressure, PsiParseError> parse_memory_pressure(std::string_view text) {
    std::optional<PsiPressureLine> some;
    std::optional<PsiPressureLine> full;
    std::size_t line_start{};

    while (line_start < text.size()) {
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line = text.substr(line_start, line_end - line_start);
        const std::vector<std::string_view> fields = split_ascii_whitespace(line);

        if (!fields.empty()) {
            const auto parsed_line = parse_pressure_line(fields);
            if (!parsed_line.has_value()) {
                return std::unexpected(parsed_line.error());
            }

            if (fields.front() == "some") {
                if (some.has_value()) {
                    return std::unexpected(
                        make_error(PsiParseErrorCode::duplicate_pressure_type,
                                   "The some pressure line appears more than once."));
                }
                some = *parsed_line;
            } else if (fields.front() == "full") {
                if (full.has_value()) {
                    return std::unexpected(
                        make_error(PsiParseErrorCode::duplicate_pressure_type,
                                   "The full pressure line appears more than once."));
                }
                full = *parsed_line;
            } else {
                return std::unexpected(make_error(PsiParseErrorCode::invalid_format,
                                                  "Pressure line type must be some or full."));
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }

        line_start = line_end + 1;
    }

    if (!some.has_value() || !full.has_value()) {
        return std::unexpected(make_error(PsiParseErrorCode::missing_pressure_type,
                                          "Both some and full pressure lines are required."));
    }

    return PsiMemoryPressure{
        .some = *some,
        .full = *full,
    };
}

} // namespace memoptima::linux::detail
