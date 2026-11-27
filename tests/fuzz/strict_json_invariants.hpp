#pragma once

#include <cstddef>
#include <cstdint>

namespace memoptima::test {

void verify_strict_json_invariants(const std::uint8_t *data, std::size_t size);

} // namespace memoptima::test
