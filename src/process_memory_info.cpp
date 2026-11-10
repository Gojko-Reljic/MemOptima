#include <memoptima/process_memory_info.hpp>

#include <utility>

namespace memoptima {

std::expected<ProcessMemoryInfo, ProcessInfoValidationError> ProcessMemoryInfo::create(
    std::uint32_t pid,
    std::string name,
    std::uint64_t resident_memory_bytes) {
    if (pid == 0) {
        return std::unexpected(ProcessInfoValidationError{
            .code = ProcessInfoValidationErrorCode::pid_must_be_positive,
        });
    }

    if (name.empty()) {
        return std::unexpected(ProcessInfoValidationError{
            .code = ProcessInfoValidationErrorCode::name_must_not_be_empty,
        });
    }

    return ProcessMemoryInfo{pid, std::move(name), resident_memory_bytes};
}

ProcessMemoryInfo::ProcessMemoryInfo(
    std::uint32_t pid,
    std::string name,
    std::uint64_t resident_memory_bytes) noexcept
    : pid_{pid}, name_{std::move(name)}, resident_memory_bytes_{resident_memory_bytes} {}

std::uint32_t ProcessMemoryInfo::pid() const noexcept {
    return pid_;
}

std::string_view ProcessMemoryInfo::name() const noexcept {
    return name_;
}

std::uint64_t ProcessMemoryInfo::resident_memory_bytes() const noexcept {
    return resident_memory_bytes_;
}

}  // namespace memoptima
