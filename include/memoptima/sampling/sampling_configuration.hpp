#pragma once

#include <chrono>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace memoptima::sampling {

enum class SamplingConfigurationErrorCode {
    count_invalid_format,
    count_out_of_range,
    interval_invalid_format,
    interval_unsupported_unit,
    interval_out_of_range,
};

struct SamplingConfigurationError {
    SamplingConfigurationErrorCode code;
    std::string detail;
};

class SamplingCount {
  public:
    [[nodiscard]] static std::expected<SamplingCount, SamplingConfigurationError>
    create(std::uint64_t value);

    [[nodiscard]] std::uint32_t value() const noexcept;

  private:
    explicit SamplingCount(std::uint32_t value) : value_{value} {}

    std::uint32_t value_;
};

class SamplingInterval {
  public:
    [[nodiscard]] static std::expected<SamplingInterval, SamplingConfigurationError>
    create(std::chrono::milliseconds value);

    [[nodiscard]] std::chrono::milliseconds value() const noexcept;

  private:
    explicit SamplingInterval(std::chrono::milliseconds value) : value_{value} {}

    std::chrono::milliseconds value_;
};

// Parsers define the stable CLI grammar rather than accepting implementation-specific numerals.
[[nodiscard]] std::expected<SamplingCount, SamplingConfigurationError>
parse_sampling_count(std::string_view text);
[[nodiscard]] std::expected<SamplingInterval, SamplingConfigurationError>
parse_sampling_interval(std::string_view text);

} // namespace memoptima::sampling
