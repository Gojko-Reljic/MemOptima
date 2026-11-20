#pragma once

#include <memoptima/sampling/output_contract.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string>

namespace memoptima::detail {
struct EvidenceInputReaderAccess;
}

namespace memoptima {

enum class EvidenceInputErrorCode {
    input_open_failed,
    input_is_symbolic_link,
    input_not_regular_file,
    input_read_failed,
    line_too_long,
    input_too_large,
};

struct EvidenceInputError {
    EvidenceInputErrorCode code;
    std::string detail;
};

class EvidenceInputReader {
  public:
    [[nodiscard]] static std::expected<EvidenceInputReader, EvidenceInputError>
    open(const std::filesystem::path &path,
         std::uint64_t maximum_line_bytes = sampling::kSamplingOutputMaximumBytes,
         std::uint64_t maximum_total_bytes = sampling::kSamplingOutputMaximumBytes);

    EvidenceInputReader(const EvidenceInputReader &) = delete;
    EvidenceInputReader &operator=(const EvidenceInputReader &) = delete;
    EvidenceInputReader(EvidenceInputReader &&other) noexcept;
    EvidenceInputReader &operator=(EvidenceInputReader &&other) noexcept;
    ~EvidenceInputReader();

    // Returns the next physical line without its newline, or nullopt when the input reaches EOF.
    [[nodiscard]] std::expected<std::optional<std::string>, EvidenceInputError> next_line();

    // Reports whether the most recently returned line ended with a newline byte.
    [[nodiscard]] bool last_line_was_terminated() const noexcept {
        return last_line_was_terminated_;
    }

  private:
    friend struct detail::EvidenceInputReaderAccess;

    EvidenceInputReader(int descriptor, std::size_t maximum_line_bytes,
                        std::size_t maximum_total_bytes) noexcept
        : descriptor_{descriptor}, maximum_line_bytes_{maximum_line_bytes},
          maximum_total_bytes_{maximum_total_bytes} {}

    [[nodiscard]] std::expected<void, EvidenceInputError>
    append_segment(std::string &line, const char *begin, std::size_t length,
                   std::size_t delimiter_bytes);

    int descriptor_ = -1;
    std::size_t maximum_line_bytes_ = 0;
    std::size_t maximum_total_bytes_ = 0;
    std::size_t total_bytes_read_ = 0;
    std::array<char, 8192> buffer_{};
    std::size_t buffer_position_ = 0;
    std::size_t buffer_size_ = 0;
    bool reached_eof_ = false;
    bool last_line_was_terminated_ = false;
};

} // namespace memoptima
