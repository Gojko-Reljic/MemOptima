#include "strict_json_invariants.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *const data, const std::size_t size) {
    memoptima::test::verify_strict_json_invariants(data, size);
    return 0;
}
