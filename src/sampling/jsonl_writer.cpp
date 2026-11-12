#include <memoptima/sampling/jsonl_writer.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace memoptima::sampling {
namespace {

[[nodiscard]] SamplingJsonlWriterError error(SamplingJsonlWriterErrorCode code,
                                             std::string detail) {
    return SamplingJsonlWriterError{.code = code, .detail = std::move(detail)};
}

[[nodiscard]] std::string errno_detail(const std::filesystem::path &path) {
    return "'" + path.string() + "': " + std::strerror(errno);
}

[[nodiscard]] bool contains_line_break(std::string_view record) noexcept {
    return record.find('\n') != std::string_view::npos ||
           record.find('\r') != std::string_view::npos;
}

[[nodiscard]] bool fits_with_reservation(std::uint64_t committed_size, std::uint64_t record_size,
                                         std::uint64_t reserve_bytes) noexcept {
    if (record_size > kSamplingOutputMaximumBytes - committed_size) {
        return false;
    }
    const std::uint64_t remaining_after_record =
        kSamplingOutputMaximumBytes - committed_size - record_size;
    return reserve_bytes <= remaining_after_record;
}

} // namespace

std::expected<SamplingJsonlWriter, SamplingJsonlWriterError>
SamplingJsonlWriter::create(const std::filesystem::path &output_path) {
    const int descriptor =
        ::open(output_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, S_IRUSR | S_IWUSR);
    if (descriptor == -1) {
        const SamplingJsonlWriterErrorCode code =
            errno == EEXIST ? SamplingJsonlWriterErrorCode::output_already_exists
                            : SamplingJsonlWriterErrorCode::output_open_failed;
        return std::unexpected(error(code, errno_detail(output_path)));
    }

    if (::fchmod(descriptor, S_IRUSR | S_IWUSR) == -1) {
        const std::string detail = errno_detail(output_path);
        (void)::close(descriptor);
        (void)::unlink(output_path.c_str());
        return std::unexpected(
            error(SamplingJsonlWriterErrorCode::output_permissions_failed, detail));
    }

    std::FILE *stream = ::fdopen(descriptor, "w");
    if (stream == nullptr) {
        const std::string detail = errno_detail(output_path);
        (void)::close(descriptor);
        (void)::unlink(output_path.c_str());
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_open_failed, detail));
    }
    return SamplingJsonlWriter{stream};
}

SamplingJsonlWriter::SamplingJsonlWriter(SamplingJsonlWriter &&other) noexcept
    : stream_{std::exchange(other.stream_, nullptr)},
      committed_size_bytes_{std::exchange(other.committed_size_bytes_, 0)} {}

SamplingJsonlWriter &SamplingJsonlWriter::operator=(SamplingJsonlWriter &&other) noexcept {
    if (this != &other) {
        if (stream_ != nullptr) {
            (void)::fclose(stream_);
        }
        stream_ = std::exchange(other.stream_, nullptr);
        committed_size_bytes_ = std::exchange(other.committed_size_bytes_, 0);
    }
    return *this;
}

SamplingJsonlWriter::~SamplingJsonlWriter() {
    if (stream_ != nullptr) {
        (void)::fclose(stream_);
    }
}

std::expected<void, SamplingJsonlWriterError>
SamplingJsonlWriter::write_record(std::string_view record, std::uint64_t reserve_bytes) {
    if (stream_ == nullptr) {
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_write_failed,
                                     "The output stream is already closed."));
    }
    if (record.empty() || contains_line_break(record)) {
        return std::unexpected(
            error(SamplingJsonlWriterErrorCode::invalid_record,
                  "A JSON Lines record must be non-empty and contain no line break."));
    }
    if (record.size() > std::numeric_limits<std::uint64_t>::max() - 1) {
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_size_limit_reached,
                                     "The JSON Lines record is too large to represent."));
    }

    const std::uint64_t record_size = static_cast<std::uint64_t>(record.size()) + 1;
    if (!fits_with_reservation(committed_size_bytes_, record_size, reserve_bytes)) {
        return std::unexpected(
            error(SamplingJsonlWriterErrorCode::output_size_limit_reached,
                  "Writing this record would exceed the configured output size limit."));
    }

    std::FILE *const stream = stream_;
    if (::fwrite(record.data(), 1, record.size(), stream) != record.size() ||
        ::fwrite("\n", 1, 1, stream) != 1) {
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_write_failed,
                                     "Unable to write a complete JSON Lines record."));
    }
    if (::fflush(stream) != 0) {
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_flush_failed,
                                     "Unable to flush the JSON Lines output stream."));
    }

    committed_size_bytes_ += record_size;
    return {};
}

std::uint64_t SamplingJsonlWriter::committed_size_bytes() const noexcept {
    return committed_size_bytes_;
}

std::uint64_t SamplingJsonlWriter::remaining_size_bytes() const noexcept {
    return kSamplingOutputMaximumBytes - committed_size_bytes_;
}

std::expected<void, SamplingJsonlWriterError> SamplingJsonlWriter::close() {
    if (stream_ == nullptr) {
        return {};
    }
    if (::fclose(stream_) != 0) {
        stream_ = nullptr;
        return std::unexpected(error(SamplingJsonlWriterErrorCode::output_flush_failed,
                                     "Unable to close the JSON Lines output stream."));
    }
    stream_ = nullptr;
    return {};
}

} // namespace memoptima::sampling
