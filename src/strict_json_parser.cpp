#include "strict_json_parser.hpp"

#include <algorithm>
#include <cctype>
#include <ranges>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace memoptima::detail {
namespace {

using Json = nlohmann::ordered_json;

enum class ContainerKind {
    object,
    array,
};

struct ContainerFrame {
    ContainerKind kind;
    std::string path;
    std::unordered_set<std::string> keys;
    std::optional<std::string> pending_key;
    std::size_t next_array_index{};
};

[[nodiscard]] bool exceeds_maximum_nesting_depth(const std::string_view text) {
    std::size_t depth = 0U;
    bool inside_string = false;
    bool escaped = false;

    for (const char character : text) {
        if (inside_string) {
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                inside_string = false;
            }
            continue;
        }

        if (character == '"') {
            inside_string = true;
        } else if (character == '{' || character == '[') {
            ++depth;
            if (depth > kStrictJsonMaximumNestingDepth) {
                return true;
            }
        } else if ((character == '}' || character == ']') && depth > 0U) {
            --depth;
        }
    }

    return false;
}

[[nodiscard]] bool is_simple_path_key(const std::string_view key) {
    if (key.empty()) {
        return false;
    }

    const auto is_identifier_start = [](const unsigned char character) {
        return std::isalpha(character) != 0 || character == '_';
    };
    const auto is_identifier_continue = [&is_identifier_start](const unsigned char character) {
        return is_identifier_start(character) || std::isdigit(character) != 0;
    };

    if (!is_identifier_start(static_cast<unsigned char>(key.front()))) {
        return false;
    }

    return std::ranges::all_of(key.substr(1), [is_identifier_continue](const char character) {
        return is_identifier_continue(static_cast<unsigned char>(character));
    });
}

[[nodiscard]] std::string escape_path_key(const std::string_view key) {
    std::string escaped;
    escaped.reserve(key.size());

    constexpr char hexadecimal_digits[] = "0123456789ABCDEF";
    for (const unsigned char character : key) {
        if (character == '\\' || character == '"') {
            escaped.push_back('\\');
            escaped.push_back(static_cast<char>(character));
        } else if (character < 0x20U || character == 0x7FU) {
            escaped += "\\u00";
            escaped.push_back(hexadecimal_digits[character >> 4U]);
            escaped.push_back(hexadecimal_digits[character & 0x0FU]);
        } else {
            escaped.push_back(static_cast<char>(character));
        }
    }

    return escaped;
}

[[nodiscard]] std::string append_property_path(const std::string_view object_path,
                                               const std::string_view key) {
    if (is_simple_path_key(key)) {
        return std::string{object_path} + "." + std::string{key};
    }

    return std::string{object_path} + "[\"" + escape_path_key(key) + "\"]";
}

[[nodiscard]] std::string append_array_path(const std::string_view array_path,
                                            const std::size_t index) {
    return std::string{array_path} + "[" + std::to_string(index) + "]";
}

[[nodiscard]] std::string consume_value_path(std::vector<ContainerFrame> &frames) {
    if (frames.empty()) {
        return "$";
    }

    ContainerFrame &parent = frames.back();
    if (parent.kind == ContainerKind::array) {
        return append_array_path(parent.path, parent.next_array_index++);
    }

    if (!parent.pending_key.has_value()) {
        return parent.path;
    }

    std::string path = append_property_path(parent.path, *parent.pending_key);
    parent.pending_key.reset();
    return path;
}

} // namespace

std::string append_json_member(const std::string_view object_path, const std::string_view key) {
    return append_property_path(object_path, key);
}

std::expected<Json, StrictJsonError> parse_strict_json(const std::string_view text) {
    if (exceeds_maximum_nesting_depth(text)) {
        return std::unexpected(StrictJsonError{.kind = StrictJsonErrorKind::nesting_too_deep,
                                               .duplicate = std::nullopt});
    }

    std::vector<ContainerFrame> frames;
    std::optional<StrictJsonDuplicateKey> duplicate;

    const Json::parser_callback_t callback = [&frames, &duplicate]([[maybe_unused]] const int depth,
                                                                   const Json::parse_event_t event,
                                                                   Json &parsed) {
        switch (event) {
        case Json::parse_event_t::object_start:
            frames.push_back(ContainerFrame{.kind = ContainerKind::object,
                                            .path = consume_value_path(frames),
                                            .keys = {},
                                            .pending_key = std::nullopt,
                                            .next_array_index = 0U});
            break;
        case Json::parse_event_t::array_start:
            frames.push_back(ContainerFrame{.kind = ContainerKind::array,
                                            .path = consume_value_path(frames),
                                            .keys = {},
                                            .pending_key = std::nullopt,
                                            .next_array_index = 0U});
            break;
        case Json::parse_event_t::key: {
            if (frames.empty() || frames.back().kind != ContainerKind::object) {
                break;
            }

            std::string key = parsed.get<std::string>();
            ContainerFrame &object = frames.back();
            if (!object.keys.insert(key).second && !duplicate.has_value()) {
                duplicate = StrictJsonDuplicateKey{.object_path = object.path, .key = key};
            }
            object.pending_key = std::move(key);
            break;
        }
        case Json::parse_event_t::value:
            static_cast<void>(consume_value_path(frames));
            break;
        case Json::parse_event_t::object_end:
        case Json::parse_event_t::array_end:
            if (!frames.empty()) {
                frames.pop_back();
            }
            break;
        }

        return true;
    };

    Json parsed = Json::parse(text, callback, false);
    if (parsed.is_discarded()) {
        return std::unexpected(
            StrictJsonError{.kind = StrictJsonErrorKind::invalid_json, .duplicate = std::nullopt});
    }
    if (duplicate.has_value()) {
        return std::unexpected(StrictJsonError{.kind = StrictJsonErrorKind::duplicate_key,
                                               .duplicate = std::move(duplicate)});
    }

    return parsed;
}

} // namespace memoptima::detail
