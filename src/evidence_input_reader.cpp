#include <memoptima/evidence_input_reader.hpp>

#include "evidence_input_reader_internal.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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

namespace detail {

struct EvidenceInputReaderAccess {
    [[nodiscard]] static std::expected<std::pair<std::size_t, std::size_t>, EvidenceInputError>
    validate_limits(std::uint64_t maximum_line_bytes, std::uint64_t maximum_total_bytes) {
        if (maximum_line_bytes > std::numeric_limits<std::size_t>::max()) {
            return std::unexpected(error(EvidenceInputErrorCode::line_too_long,
                                         "The configured line limit cannot be represented."));
        }
        if (maximum_total_bytes > std::numeric_limits<std::size_t>::max()) {
            return std::unexpected(
                error(EvidenceInputErrorCode::input_too_large,
                      "The configured total input limit cannot be represented."));
        }
        return std::pair{static_cast<std::size_t>(maximum_line_bytes),
                         static_cast<std::size_t>(maximum_total_bytes)};
    }

    [[nodiscard]] static std::expected<EvidenceInputReader, EvidenceInputError>
    make_reader(int descriptor, const std::filesystem::path &display_path,
                std::size_t maximum_line_bytes, std::size_t maximum_total_bytes) {
        struct stat status{};
        if (::fstat(descriptor, &status) == -1) {
            const std::string detail = errno_detail(display_path);
            (void)::close(descriptor);
            return std::unexpected(error(EvidenceInputErrorCode::input_open_failed, detail));
        }
        if (!S_ISREG(status.st_mode)) {
            (void)::close(descriptor);
            return std::unexpected(error(EvidenceInputErrorCode::input_not_regular_file,
                                         "The input path must name a regular file."));
        }
        if (status.st_size < 0 ||
            static_cast<std::uintmax_t>(status.st_size) > maximum_total_bytes) {
            (void)::close(descriptor);
            return std::unexpected(
                error(EvidenceInputErrorCode::input_too_large,
                      "The evidence artifact exceeds the configured size limit."));
        }
        return EvidenceInputReader{descriptor, maximum_line_bytes, maximum_total_bytes};
    }
};

std::expected<EvidenceInputReader, EvidenceInputError>
open_evidence_input_beneath(const std::filesystem::path &root,
                            const std::filesystem::path &relative_path,
                            std::uint64_t maximum_line_bytes, std::uint64_t maximum_total_bytes) {
    const auto limits =
        EvidenceInputReaderAccess::validate_limits(maximum_line_bytes, maximum_total_bytes);
    if (!limits.has_value()) {
        return std::unexpected(limits.error());
    }
    if (relative_path.empty() || relative_path.is_absolute()) {
        return std::unexpected(error(EvidenceInputErrorCode::input_open_failed,
                                     "The artifact path must be non-empty and relative."));
    }

    std::vector<std::string> components;
    for (const auto &component : relative_path) {
        const std::string text = component.string();
        if (text.empty() || text.find('\0') != std::string::npos || text == "." || text == "..") {
            return std::unexpected(error(EvidenceInputErrorCode::input_open_failed,
                                         "The artifact path contains an unsafe component."));
        }
        components.push_back(text);
    }
    if (components.empty()) {
        return std::unexpected(error(EvidenceInputErrorCode::input_open_failed,
                                     "The artifact path must name a file."));
    }

    int descriptor = ::open(root.c_str(), O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (descriptor == -1) {
        const EvidenceInputErrorCode code = errno == ELOOP
                                                ? EvidenceInputErrorCode::input_is_symbolic_link
                                                : EvidenceInputErrorCode::input_open_failed;
        return std::unexpected(error(code, errno_detail(root)));
    }

    for (std::size_t index{}; index < components.size(); ++index) {
        const bool final_component = index + 1U == components.size();
        const int flags = final_component ? O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK
                                          : O_RDONLY | O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW;
        const int next_descriptor = ::openat(descriptor, components[index].c_str(), flags);
        const std::filesystem::path attempted_path = root / relative_path;
        if (next_descriptor == -1) {
            const EvidenceInputErrorCode code = errno == ELOOP
                                                    ? EvidenceInputErrorCode::input_is_symbolic_link
                                                    : EvidenceInputErrorCode::input_open_failed;
            const std::string detail = errno_detail(attempted_path);
            (void)::close(descriptor);
            return std::unexpected(error(code, detail));
        }
        (void)::close(descriptor);
        descriptor = next_descriptor;
    }

    return EvidenceInputReaderAccess::make_reader(descriptor, root / relative_path, limits->first,
                                                  limits->second);
}

} // namespace detail

std::expected<EvidenceInputReader, EvidenceInputError>
EvidenceInputReader::open(const std::filesystem::path &path, std::uint64_t maximum_line_bytes,
                          std::uint64_t maximum_total_bytes) {
    const auto limits =
        detail::EvidenceInputReaderAccess::validate_limits(maximum_line_bytes, maximum_total_bytes);
    if (!limits.has_value()) {
        return std::unexpected(limits.error());
    }

    // Do not follow links or block on special files before fstat confirms a regular artifact.
    const int descriptor = ::open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (descriptor == -1) {
        const EvidenceInputErrorCode code = errno == ELOOP
                                                ? EvidenceInputErrorCode::input_is_symbolic_link
                                                : EvidenceInputErrorCode::input_open_failed;
        return std::unexpected(error(code, errno_detail(path)));
    }

    return detail::EvidenceInputReaderAccess::make_reader(descriptor, path, limits->first,
                                                          limits->second);
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
