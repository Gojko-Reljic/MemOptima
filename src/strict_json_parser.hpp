#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace memoptima::detail {

inline constexpr std::size_t kStrictJsonMaximumNestingDepth = 64U;

struct StrictJsonDuplicateKey {
    std::string object_path;
    std::string key;
};

enum class StrictJsonErrorKind {
    nesting_too_deep,
    invalid_json,
    duplicate_key,
};

struct StrictJsonError {
    StrictJsonErrorKind kind;
    std::optional<StrictJsonDuplicateKey> duplicate;
};

// Parses untrusted JSON while enforcing limits that a completed JSON DOM cannot recover.
[[nodiscard]] std::expected<nlohmann::ordered_json, StrictJsonError>
parse_strict_json(std::string_view text);

} // namespace memoptima::detail
