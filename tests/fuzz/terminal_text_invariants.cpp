#include "terminal_text_invariants.hpp"

#include "cli/formatting.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace memoptima::test {
namespace {

struct DecodedUtf8 {
    std::uint32_t code_point;
    std::size_t byte_count;
};

[[noreturn]] void invariant_failure() { __builtin_trap(); }

[[nodiscard]] bool is_continuation_byte(const unsigned char byte) {
    return (byte & 0xC0U) == 0x80U;
}

[[nodiscard]] std::optional<DecodedUtf8> decode_utf8(const std::string_view value,
                                                     const std::size_t offset) {
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
        if (!is_continuation_byte(next)) {
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

[[nodiscard]] bool is_terminal_control(const std::uint32_t code_point) {
    return code_point <= 0x1FU || code_point == 0x7FU ||
           (code_point >= 0x80U && code_point <= 0x9FU) || code_point == 0x61CU ||
           (code_point >= 0x200BU && code_point <= 0x200DU) || code_point == 0x200EU ||
           code_point == 0x200FU || (code_point >= 0x2028U && code_point <= 0x202EU) ||
           code_point == 0x2060U || (code_point >= 0x2066U && code_point <= 0x2069U) ||
           code_point == 0xFEFFU;
}

} // namespace

void verify_terminal_text_invariants(const std::uint8_t *const data, const std::size_t size) {
    const char *const bytes = size == 0U ? "" : reinterpret_cast<const char *>(data);
    const std::string escaped = cli::escape_terminal_text(std::string_view{bytes, size});

    for (std::size_t offset{}; offset < escaped.size();) {
        const auto decoded = decode_utf8(escaped, offset);
        if (!decoded.has_value() || is_terminal_control(decoded->code_point)) {
            invariant_failure();
        }
        offset += decoded->byte_count;
    }
}

} // namespace memoptima::test
