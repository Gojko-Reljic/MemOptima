#include "strict_json_invariants.hpp"

#include "strict_json_parser.hpp"

#include <nlohmann/json.hpp>

#include <string_view>
#include <utility>
#include <vector>

namespace memoptima::test {
namespace {

using Json = nlohmann::ordered_json;

[[noreturn]] void invariant_failure() { __builtin_trap(); }

void verify_json_depth(const Json &root) {
    struct PendingValue {
        const Json *value;
        std::size_t depth;
    };

    std::vector<PendingValue> pending;
    if (root.is_array() || root.is_object()) {
        pending.push_back(PendingValue{.value = &root, .depth = 1U});
    }

    while (!pending.empty()) {
        const PendingValue current = pending.back();
        pending.pop_back();
        if (current.depth > detail::kStrictJsonMaximumNestingDepth) {
            invariant_failure();
        }
        for (const auto &item : current.value->items()) {
            const Json &child = item.value();
            if (child.is_array() || child.is_object()) {
                pending.push_back(PendingValue{.value = &child, .depth = current.depth + 1U});
            }
        }
    }
}

} // namespace

void verify_strict_json_invariants(const std::uint8_t *const data, const std::size_t size) {
    const char *const bytes = size == 0U ? "" : reinterpret_cast<const char *>(data);
    const std::string_view input{bytes, size};
    const auto strict = detail::parse_strict_json(input);
    const Json ordinary = Json::parse(input, nullptr, false);

    if (strict.has_value()) {
        if (ordinary.is_discarded() || *strict != ordinary) {
            invariant_failure();
        }

        const auto round_trip = detail::parse_strict_json(strict->dump());
        if (!round_trip.has_value() || *round_trip != *strict) {
            invariant_failure();
        }

        verify_json_depth(*strict);
        return;
    }

    switch (strict.error().kind) {
    case detail::StrictJsonErrorKind::nesting_too_deep:
        return;
    case detail::StrictJsonErrorKind::invalid_json:
        if (!ordinary.is_discarded()) {
            invariant_failure();
        }
        return;
    case detail::StrictJsonErrorKind::duplicate_key:
        if (ordinary.is_discarded()) {
            invariant_failure();
        }
        return;
    }

    invariant_failure();
}

} // namespace memoptima::test
