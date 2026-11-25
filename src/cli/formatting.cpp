#include "cli/formatting.hpp"

#include <array>
#include <format>
#include <optional>
#include <string_view>

namespace memoptima::cli {
namespace {

constexpr std::uint64_t kBytesPerUnit = 1024;
constexpr std::array<std::string_view, 5> kByteUnitNames{"B", "KiB", "MiB", "GiB", "TiB"};
constexpr std::string_view kHexadecimalDigits{"0123456789ABCDEF"};

struct DecodedUtf8 {
    std::uint32_t code_point;
    std::size_t byte_count;
};

[[nodiscard]] bool is_utf8_continuation_byte(unsigned char value) {
    return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] std::optional<DecodedUtf8> decode_utf8(std::string_view value, std::size_t offset) {
    const auto first = static_cast<unsigned char>(value[offset]);
    if (first <= 0x7FU) {
        return DecodedUtf8{.code_point = first, .byte_count = 1U};
    }

    std::size_t byte_count{};
    std::uint32_t code_point{};
    if (first >= 0xC2U && first <= 0xDFU) {
        byte_count = 2U;
        code_point = first & 0x1FU;
    } else if (first >= 0xE0U && first <= 0xEFU) {
        byte_count = 3U;
        code_point = first & 0x0FU;
    } else if (first >= 0xF0U && first <= 0xF4U) {
        byte_count = 4U;
        code_point = first & 0x07U;
    } else {
        return std::nullopt;
    }
    if (value.size() - offset < byte_count) {
        return std::nullopt;
    }
    for (std::size_t index{1U}; index < byte_count; ++index) {
        const auto next = static_cast<unsigned char>(value[offset + index]);
        if (!is_utf8_continuation_byte(next)) {
            return std::nullopt;
        }
        code_point = (code_point << 6U) | (next & 0x3FU);
    }

    const bool overlong = (byte_count == 2U && code_point < 0x80U) ||
                          (byte_count == 3U && code_point < 0x800U) ||
                          (byte_count == 4U && code_point < 0x10000U);
    const bool surrogate = code_point >= 0xD800U && code_point <= 0xDFFFU;
    if (overlong || surrogate || code_point > 0x10FFFFU) {
        return std::nullopt;
    }
    return DecodedUtf8{.code_point = code_point, .byte_count = byte_count};
}

[[nodiscard]] bool requires_unicode_escape(std::uint32_t code_point) {
    return (code_point >= 0x80U && code_point <= 0x9FU) || code_point == 0x61CU ||
           (code_point >= 0x200BU && code_point <= 0x200DU) || code_point == 0x200EU ||
           code_point == 0x200FU || (code_point >= 0x2028U && code_point <= 0x202EU) ||
           code_point == 0x2060U || (code_point >= 0x2066U && code_point <= 0x2069U) ||
           code_point == 0xFEFFU;
}

void append_byte_escape(std::string &output, unsigned char byte) {
    output += "\\x";
    output += kHexadecimalDigits[byte >> 4U];
    output += kHexadecimalDigits[byte & 0x0FU];
}

} // namespace

std::string format_bytes(std::uint64_t bytes) {
    if (bytes == 0)
        return "0 B";

    double display_value = static_cast<double>(bytes);
    std::size_t unit_index{};
    while (display_value >= static_cast<double>(kBytesPerUnit) &&
           unit_index + 1 < kByteUnitNames.size()) {
        display_value /= static_cast<double>(kBytesPerUnit);
        ++unit_index;
    }
    return std::format("{:.2f} {}", display_value, kByteUnitNames[unit_index]);
}

std::string format_percentage(double value, std::size_t decimal_places) {
    return std::format("{:.{}f}%", value, decimal_places);
}

std::string format_duration(std::uint64_t microseconds) {
    if (microseconds < 1000)
        return std::format("{} µs", microseconds);

    const double milliseconds = static_cast<double>(microseconds) / 1000.0;
    return milliseconds < 1000.0 ? std::format("{:.2f} ms", milliseconds)
                                 : std::format("{:.2f} s", milliseconds / 1000.0);
}

std::string format_operational_duration(std::chrono::milliseconds duration) {
    using namespace std::chrono;

    if (duration < seconds{1})
        return "< 1s";

    const auto total_seconds = static_cast<std::uint64_t>(duration_cast<seconds>(duration).count());
    constexpr std::uint64_t kSecondsPerMinute = 60;
    constexpr std::uint64_t kSecondsPerHour = 60 * kSecondsPerMinute;
    constexpr std::uint64_t kSecondsPerDay = 24 * kSecondsPerHour;

    if (total_seconds < kSecondsPerMinute)
        return std::format("{}s", total_seconds);

    const auto minutes = total_seconds / kSecondsPerMinute;
    const auto seconds = total_seconds % kSecondsPerMinute;
    if (total_seconds < kSecondsPerHour)
        return std::format("{}m {:02d}s", minutes, seconds);

    const auto hours = total_seconds / kSecondsPerHour;
    const auto remaining_minutes = (total_seconds % kSecondsPerHour) / kSecondsPerMinute;
    if (total_seconds < kSecondsPerDay)
        return std::format("{}h {:02d}m", hours, remaining_minutes);

    const auto days = total_seconds / kSecondsPerDay;
    const auto remaining_hours = (total_seconds % kSecondsPerDay) / kSecondsPerHour;
    return std::format("{}d {:02d}h", days, remaining_hours);
}

std::string escape_terminal_text(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (std::size_t offset{}; offset < value.size();) {
        const auto character = value[offset];
        switch (character) {
        case '\\':
            escaped += "\\\\";
            ++offset;
            break;
        case '\n':
            escaped += "\\n";
            ++offset;
            break;
        case '\r':
            escaped += "\\r";
            ++offset;
            break;
        case '\t':
            escaped += "\\t";
            ++offset;
            break;
        default: {
            const auto byte = static_cast<unsigned char>(character);
            if (byte <= 0x1FU || byte == 0x7FU) {
                append_byte_escape(escaped, byte);
                ++offset;
            } else {
                const auto decoded = decode_utf8(value, offset);
                if (!decoded.has_value()) {
                    append_byte_escape(escaped, byte);
                    ++offset;
                } else if (requires_unicode_escape(decoded->code_point)) {
                    escaped += std::format("\\u{{{:X}}}", decoded->code_point);
                    offset += decoded->byte_count;
                } else {
                    escaped.append(value.substr(offset, decoded->byte_count));
                    offset += decoded->byte_count;
                }
            }
            break;
        }
        }
    }
    return escaped;
}

} // namespace memoptima::cli
