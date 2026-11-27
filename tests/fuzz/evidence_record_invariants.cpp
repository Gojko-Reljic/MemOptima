#include "evidence_record_invariants.hpp"

#include <memoptima/evidence_record_parser.hpp>

#include <string_view>

namespace memoptima::test {
namespace {
[[noreturn]] void fail() { __builtin_trap(); }
} // namespace

void verify_evidence_record_invariants(const std::uint8_t *const data, const std::size_t size) {
    const char *const bytes = size == 0U ? "" : reinterpret_cast<const char *>(data);
    const auto parsed = parse_evidence_record(std::string_view{bytes, size}, 17U);
    if (!parsed.has_value()) {
        if (parsed.error().empty() || parsed.error().size() > 20U)
            fail();
        for (const auto &error : parsed.error())
            if (error.line_number != 17U || error.detail.empty())
                fail();
        return;
    }
    if (parsed->canonical_json.empty())
        fail();
    const auto round_trip = parse_evidence_record(parsed->canonical_json, 18U);
    if (!round_trip.has_value() || round_trip->type != parsed->type ||
        round_trip->canonical_json != parsed->canonical_json)
        fail();
}
} // namespace memoptima::test
