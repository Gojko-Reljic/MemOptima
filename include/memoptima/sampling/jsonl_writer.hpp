#pragma once

#include <memoptima/sampling/output_contract.hpp>

#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>

namespace memoptima::sampling {

enum class SamplingJsonlWriterErrorCode {
    output_already_exists,
    output_open_failed,
    output_permissions_failed,
    invalid_record,
    output_size_limit_reached,
    output_write_failed,
    output_flush_failed,
};

struct SamplingJsonlWriterError {
    SamplingJsonlWriterErrorCode code;
    std::string detail;
};

class SamplingJsonlWriter {
  public:
    [[nodiscard]] static std::expected<SamplingJsonlWriter, SamplingJsonlWriterError>
    create(const std::filesystem::path &output_path);

    SamplingJsonlWriter(const SamplingJsonlWriter &) = delete;
    SamplingJsonlWriter &operator=(const SamplingJsonlWriter &) = delete;
    SamplingJsonlWriter(SamplingJsonlWriter &&other) noexcept;
    SamplingJsonlWriter &operator=(SamplingJsonlWriter &&other) noexcept;
    ~SamplingJsonlWriter();

    // reserve_bytes is retained for a future terminal record before this record is written.
    [[nodiscard]] std::expected<void, SamplingJsonlWriterError>
    write_record(std::string_view record, std::uint64_t reserve_bytes = 0);

    [[nodiscard]] std::uint64_t committed_size_bytes() const noexcept;
    [[nodiscard]] std::uint64_t remaining_size_bytes() const noexcept;

    [[nodiscard]] std::expected<void, SamplingJsonlWriterError> close();

  private:
    explicit SamplingJsonlWriter(std::FILE *stream) : stream_{stream} {}

    std::FILE *stream_ = nullptr;
    std::uint64_t committed_size_bytes_ = 0;
};

} // namespace memoptima::sampling
