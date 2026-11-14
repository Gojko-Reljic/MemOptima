#pragma once

#include <cstdint>

namespace memoptima::sampling {

// Bounds disk consumption of a long-running sampling session, including its terminal record.
inline constexpr std::uint64_t kSamplingOutputMaximumBytes = 512ULL * 1024ULL * 1024ULL;

} // namespace memoptima::sampling
