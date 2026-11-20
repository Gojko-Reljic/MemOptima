#include <memoptima/evidence_input_reader.hpp>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <utility>

#include <sys/stat.h>
#include <unistd.h>

namespace memoptima {
namespace {

[[nodiscard]] EvidenceInputError error(EvidenceInputErrorCode code, std::string detail) {
    return EvidenceInputError{.code = code, .detail = std::move(detail)};
}

[[nodiscard]] std::string errno_detail(const std::filesystem::path &path) {
    return "'" + path.string() + "': " + std::strerror(errno);
}

} // namespace

std::expected<EvidenceInputReader, EvidenceInputError>
EvidenceInputReader::open(const std::filesystem::path &path, std::uint64_t maximum_line_bytes,
                          std::uint64_t maximum_total_bytes) {
    if (maximum_line_bytes > std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(error(EvidenceInputErrorCode::line_too_long,
                                     "The configured line limit cannot be represented."));
    }
    if (maximum_total_bytes > std::numeric_limits<std::size_t>::max()) {
        return std::unexpected(error(EvidenceInputErrorCode::input_too_large,
                                     "The configured total input limit cannot be represented."));
    }

    // Do not follow links or block on special files before fstat confirms a regular artifact.
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor == -1) {
        const EvidenceInputErrorCode code = errno == ELOOP
                                                ? EvidenceInputErrorCode::input_is_symbolic_link
                                                : EvidenceInputErrorCode::input_open_failed;
        return std::unexpected(error(code, errno_detail(path)));
    }

    struct stat status{};
    if (::fstat(descriptor, &status) == -1) {
        const std::string detail = errno_detail(path);
        (void)::close(descriptor);
        return std::unexpected(error(EvidenceInputErrorCode::input_open_failed, detail));
    }
    if (!S_ISREG(status.st_mode)) {
        (void)::close(descriptor);
        return std::unexpected(error(EvidenceInputErrorCode::input_not_regular_file,
                                     "The input path must name a regular file."));
    }

    const auto total_limit = static_cast<std::size_t>(maximum_total_bytes);
    if (status.st_size < 0 || static_cast<std::uintmax_t>(status.st_size) > total_limit) {
        (void)::close(descriptor);
        return std::unexpected(error(EvidenceInputErrorCode::input_too_large,
                                     "The evidence artifact exceeds the configured size limit."));
    }

    return EvidenceInputReader{descriptor, static_cast<std::size_t>(maximum_line_bytes),
                               total_limit};
}

EvidenceInputReader::EvidenceInputReader(EvidenceInputReader &&other) noexcept
    : descriptor_{std::exchange(other.descriptor_, -1)},
      maximum_line_bytes_{other.maximum_line_bytes_},
      maximum_total_bytes_{other.maximum_total_bytes_}, total_bytes_read_{other.total_bytes_read_},
      buffer_{other.buffer_}, buffer_position_{other.buffer_position_},
      buffer_size_{other.buffer_size_}, reached_eof_{other.reached_eof_},
      last_line_was_terminated_{other.last_line_was_terminated_} {}

EvidenceInputReader &EvidenceInputReader::operator=(EvidenceInputReader &&other) noexcept {
    if (this != &other) {
        if (descriptor_ != -1) {
            (void)::close(descriptor_);
        }
        descriptor_ = std::exchange(other.descriptor_, -1);
        maximum_line_bytes_ = other.maximum_line_bytes_;
        maximum_total_bytes_ = other.maximum_total_bytes_;
        total_bytes_read_ = other.total_bytes_read_;
        buffer_ = other.buffer_;
        buffer_position_ = other.buffer_position_;
        buffer_size_ = other.buffer_size_;
        reached_eof_ = other.reached_eof_;
        last_line_was_terminated_ = other.last_line_was_terminated_;
    }
    return *this;
}

EvidenceInputReader::~EvidenceInputReader() {
    if (descriptor_ != -1) {
        (void)::close(descriptor_);
    }
}

std::expected<void, EvidenceInputError>
EvidenceInputReader::append_segment(std::string &line, const char *begin, std::size_t length,
                                    std::size_t delimiter_bytes) {
    // Subtract before appending so an oversized untrusted record cannot wrap the size calculation.
    if (length > maximum_line_bytes_ - line.size()) {
        return std::unexpected(error(EvidenceInputErrorCode::line_too_long,
                                     "A JSON Lines record exceeds the configured size limit."));
    }
    const std::size_t physical_bytes = length + delimiter_bytes;
    if (physical_bytes > maximum_total_bytes_ - total_bytes_read_) {
        return std::unexpected(error(EvidenceInputErrorCode::input_too_large,
                                     "The evidence artifact exceeds the configured size limit."));
    }
    line.append(begin, length);
    total_bytes_read_ += physical_bytes;
    return {};
}

std::expected<std::optional<std::string>, EvidenceInputError> EvidenceInputReader::next_line() {
    std::string line;
    while (true) {
        if (buffer_position_ < buffer_size_) {
            const char *const segment_begin = buffer_.data() + buffer_position_;
            const void *const newline =
                std::memchr(segment_begin, '\n', buffer_size_ - buffer_position_);
            const std::size_t segment_length =
                newline == nullptr ? buffer_size_ - buffer_position_
                                   : static_cast<const char *>(newline) - segment_begin;
            const auto append_result =
                append_segment(line, segment_begin, segment_length, newline == nullptr ? 0U : 1U);
            if (!append_result.has_value()) {
                return std::unexpected(append_result.error());
            }
            buffer_position_ += segment_length;
            if (newline != nullptr) {
                ++buffer_position_;
                last_line_was_terminated_ = true;
                return line;
            }
        }

        if (reached_eof_) {
            if (line.empty()) {
                last_line_was_terminated_ = false;
                return std::optional<std::string>{};
            }
            last_line_was_terminated_ = false;
            return line;
        }

        const ssize_t bytes_read = ::read(descriptor_, buffer_.data(), buffer_.size());
        if (bytes_read == -1) {
            if (errno == EINTR) {
                // A signal interrupted the syscall before it produced input; retry the same read.
                continue;
            }
            return std::unexpected(error(EvidenceInputErrorCode::input_read_failed,
                                         "Unable to read the evidence artifact."));
        }
        buffer_position_ = 0;
        buffer_size_ = static_cast<std::size_t>(bytes_read);
        reached_eof_ = bytes_read == 0;
    }
}

} // namespace memoptima
