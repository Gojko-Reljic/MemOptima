#include <memoptima/sampling/sampling_configuration.hpp>

#include <charconv>
#include <limits>

namespace memoptima::sampling {
namespace {

constexpr std::uint64_t kMinimumSampleCount = 1;
constexpr std::uint64_t kMaximumSampleCount = 3600;
constexpr std::chrono::milliseconds kMinimumSamplingInterval{100};
constexpr std::chrono::milliseconds kMaximumSamplingInterval{60 * 60 * 1000};

[[nodiscard]] SamplingConfigurationError count_invalid_format_error() {
    return SamplingConfigurationError{
        .code = SamplingConfigurationErrorCode::count_invalid_format,
        .detail =
            "Sampling count must be a canonical positive decimal integer without leading zeros.",
    };
}

[[nodiscard]] SamplingConfigurationError count_out_of_range_error() {
    return SamplingConfigurationError{
        .code = SamplingConfigurationErrorCode::count_out_of_range,
        .detail = "Sampling count must be between 1 and 3600.",
    };
}

[[nodiscard]] SamplingConfigurationError interval_invalid_format_error() {
    return SamplingConfigurationError{
        .code = SamplingConfigurationErrorCode::interval_invalid_format,
        .detail = "Sampling interval must be a canonical decimal integer followed by a unit.",
    };
}

[[nodiscard]] SamplingConfigurationError interval_unsupported_unit_error() {
    return SamplingConfigurationError{
        .code = SamplingConfigurationErrorCode::interval_unsupported_unit,
        .detail = "Sampling interval unit must be one of: ms, s, m.",
    };
}

[[nodiscard]] SamplingConfigurationError interval_out_of_range_error() {
    return SamplingConfigurationError{
        .code = SamplingConfigurationErrorCode::interval_out_of_range,
        .detail = "Sampling interval must be between 100ms and 60m.",
    };
}

[[nodiscard]] bool is_ascii_decimal_digit(char character) noexcept {
    return character >= '0' && character <= '9';
}

[[nodiscard]] bool is_ascii_letter(char character) noexcept {
    return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
}

enum class DecimalParseError {
    invalid_format,
    overflow,
};

[[nodiscard]] std::expected<std::uint64_t, DecimalParseError>
parse_canonical_decimal(std::string_view text) noexcept {
    // from_chars parses efficiently, while this explicit check preserves the CLI's canonical
    // grammar.
    if (text.empty() || (text.size() > 1 && text.front() == '0')) {
        return std::unexpected(DecimalParseError::invalid_format);
    }

    for (const char character : text) {
        if (!is_ascii_decimal_digit(character)) {
            return std::unexpected(DecimalParseError::invalid_format);
        }
    }

    std::uint64_t value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error == std::errc::result_out_of_range) {
        return std::unexpected(DecimalParseError::overflow);
    }
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::unexpected(DecimalParseError::invalid_format);
    }
    return value;
}

[[nodiscard]] std::expected<std::chrono::milliseconds, DecimalParseError>
make_interval(std::uint64_t magnitude, std::uint64_t milliseconds_per_unit) noexcept {
    // chrono::milliseconds uses a signed representation, so validate before multiplication and
    // cast.
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) /
                        milliseconds_per_unit) {
        return std::unexpected(DecimalParseError::overflow);
    }
    return std::chrono::milliseconds{static_cast<std::int64_t>(magnitude * milliseconds_per_unit)};
}

} // namespace

std::expected<SamplingCount, SamplingConfigurationError>
SamplingCount::create(std::uint64_t value) {
    if (value < kMinimumSampleCount || value > kMaximumSampleCount) {
        return std::unexpected(count_out_of_range_error());
    }
    return SamplingCount{static_cast<std::uint32_t>(value)};
}

std::uint32_t SamplingCount::value() const noexcept { return value_; }

std::expected<SamplingInterval, SamplingConfigurationError>
SamplingInterval::create(std::chrono::milliseconds value) {
    if (value < kMinimumSamplingInterval || value > kMaximumSamplingInterval) {
        return std::unexpected(interval_out_of_range_error());
    }
    return SamplingInterval{value};
}

std::chrono::milliseconds SamplingInterval::value() const noexcept { return value_; }

std::expected<SamplingCount, SamplingConfigurationError>
parse_sampling_count(std::string_view text) {
    const auto value = parse_canonical_decimal(text);
    if (!value.has_value()) {
        return std::unexpected(value.error() == DecimalParseError::overflow
                                   ? count_out_of_range_error()
                                   : count_invalid_format_error());
    }
    return SamplingCount::create(*value);
}

std::expected<SamplingInterval, SamplingConfigurationError>
parse_sampling_interval(std::string_view text) {
    std::size_t magnitude_length{};
    while (magnitude_length < text.size() && is_ascii_decimal_digit(text[magnitude_length])) {
        ++magnitude_length;
    }
    if (magnitude_length == 0) {
        return std::unexpected(interval_invalid_format_error());
    }

    const std::string_view magnitude_text = text.substr(0, magnitude_length);
    const std::string_view unit = text.substr(magnitude_length);
    std::uint64_t milliseconds_per_unit{};
    if (unit == "ms") {
        milliseconds_per_unit = 1;
    } else if (unit == "s") {
        milliseconds_per_unit = 1000;
    } else if (unit == "m") {
        milliseconds_per_unit = 60 * 1000;
    } else if (!unit.empty()) {
        for (const char character : unit) {
            if (!is_ascii_letter(character)) {
                return std::unexpected(interval_invalid_format_error());
            }
        }
        return std::unexpected(interval_unsupported_unit_error());
    } else {
        return std::unexpected(interval_unsupported_unit_error());
    }

    const auto magnitude = parse_canonical_decimal(magnitude_text);
    if (!magnitude.has_value()) {
        return std::unexpected(magnitude.error() == DecimalParseError::overflow
                                   ? interval_out_of_range_error()
                                   : interval_invalid_format_error());
    }

    const auto interval = make_interval(*magnitude, milliseconds_per_unit);
    if (!interval.has_value()) {
        return std::unexpected(interval_out_of_range_error());
    }
    return SamplingInterval::create(*interval);
}

} // namespace memoptima::sampling
