#include "evidence_record_invariants.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, const std::size_t size) {
    memoptima::test::verify_evidence_record_invariants(data, size);
    return 0;
}
