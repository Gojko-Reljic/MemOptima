#include "linux/process_status_parser.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace memoptima::linux::detail {
namespace {

constexpr std::uint64_t kBytesPerKilobyte = 1024;

struct FieldValue {
    std::string_view text;
    std::size_t line_number;
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

[[nodiscard]] ProcessReadError make_error(
    ProcessReadErrorCode code,
    std::string_view field_name,
    std::size_t line_number,
    std::string_view detail) {
    return {
        .code = code,
        .field_name = std::string{field_name},
        .line_number = line_number,
        .detail = std::string{detail},
    };
}

[[nodiscard]] std::expected<std::uint64_t, ProcessReadError> parse_rss_in_bytes(
    std::string_view value_and_unit,
    std::size_t line_number) {
    const std::string_view trimmed_value = trim_ascii_whitespace(value_and_unit);
    const std::size_t value_end = trimmed_value.find_first_of(" \t\r");
    const std::string_view value_text = trimmed_value.substr(0, value_end);

    if (value_text.empty()) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::invalid_value,
            "VmRSS",
            line_number,
            "Expected an unsigned integer value."));
    }

    std::uint64_t value_in_kilobytes{};
    const auto [parse_end, parse_error] = std::from_chars(
        value_text.data(), value_text.data() + value_text.size(), value_in_kilobytes);

    if (parse_error == std::errc::result_out_of_range) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::value_overflow,
            "VmRSS",
            line_number,
            "The value does not fit in an unsigned 64-bit integer."));
    }

    if (parse_error != std::errc{} || parse_end != value_text.data() + value_text.size()) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::invalid_value,
            "VmRSS",
            line_number,
            "Expected an unsigned integer value."));
    }

    const std::string_view unit = trim_ascii_whitespace(trimmed_value.substr(value_end));
    if (unit != "kB") {
        return std::unexpected(make_error(
            ProcessReadErrorCode::unexpected_unit,
            "VmRSS",
            line_number,
            "Expected the unit kB."));
    }

    if (value_in_kilobytes > std::numeric_limits<std::uint64_t>::max() / kBytesPerKilobyte) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::value_overflow,
            "VmRSS",
            line_number,
            "The value cannot be converted from kB to bytes without overflow."));
    }

    return value_in_kilobytes * kBytesPerKilobyte;
}

[[nodiscard]] std::expected<FieldValue, ProcessReadError> require_field(
    const std::optional<FieldValue>& field,
    std::string_view field_name) {
    if (!field.has_value()) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::missing_required_field,
            field_name,
            0,
            "The required field is missing."));
    }

    return *field;
}

}  // namespace

std::expected<ProcessMemoryInfo, ProcessReadError> parse_process_status(
    std::uint32_t pid,
    std::string_view text) {
    std::optional<FieldValue> name;
    std::optional<FieldValue> resident_memory;
    std::size_t line_number{};
    std::size_t line_start{};

    while (line_start < text.size()) {
        ++line_number;
        const std::size_t line_end = text.find('\n', line_start);
        const std::string_view line = text.substr(line_start, line_end - line_start);
        const std::size_t separator = line.find(':');

        if (separator != std::string_view::npos) {
            const std::string_view field_name = line.substr(0, separator);
            const FieldValue field_value{
                .text = trim_ascii_whitespace(line.substr(separator + 1)),
                .line_number = line_number,
            };

            if (field_name == "Name") {
                if (name.has_value()) {
                    return std::unexpected(make_error(
                        ProcessReadErrorCode::duplicate_required_field,
                        field_name,
                        line_number,
                        "The field appears more than once."));
                }
                name = field_value;
            } else if (field_name == "VmRSS") {
                if (resident_memory.has_value()) {
                    return std::unexpected(make_error(
                        ProcessReadErrorCode::duplicate_required_field,
                        field_name,
                        line_number,
                        "The field appears more than once."));
                }
                resident_memory = field_value;
            }
        }

        if (line_end == std::string_view::npos) {
            break;
        }

        line_start = line_end + 1;
    }

    const auto process_name = require_field(name, "Name");
    if (!process_name.has_value()) {
        return std::unexpected(process_name.error());
    }

    if (process_name->text.empty()) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::invalid_process_info,
            "Name",
            process_name->line_number,
            "The process name must not be empty."));
    }

    const auto rss_field = require_field(resident_memory, "VmRSS");
    if (!rss_field.has_value()) {
        return std::unexpected(rss_field.error());
    }

    const auto resident_memory_bytes = parse_rss_in_bytes(rss_field->text, rss_field->line_number);
    if (!resident_memory_bytes.has_value()) {
        return std::unexpected(resident_memory_bytes.error());
    }

    const auto process = ProcessMemoryInfo::create(
        pid,
        std::string{process_name->text},
        *resident_memory_bytes);
    if (process.has_value()) {
        return *process;
    }

    if (process.error().code == ProcessInfoValidationErrorCode::pid_must_be_positive) {
        return std::unexpected(make_error(
            ProcessReadErrorCode::invalid_process_info,
            "Pid",
            0,
            "The process ID must be positive."));
    }

    return std::unexpected(make_error(
        ProcessReadErrorCode::invalid_process_info,
        "Name",
        process_name->line_number,
        "The process name must not be empty."));
}

}  // namespace memoptima::linux::detail
