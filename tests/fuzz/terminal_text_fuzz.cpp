#include "terminal_text_invariants.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *const data, const std::size_t size) {
    memoptima::test::verify_terminal_text_invariants(data, size);
    return 0;
}
