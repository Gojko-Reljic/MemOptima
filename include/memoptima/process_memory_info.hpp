#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace memoptima {

enum class ProcessInfoValidationErrorCode {
    pid_must_be_positive,
    name_must_not_be_empty,
};

struct ProcessInfoValidationError {
    ProcessInfoValidationErrorCode code;
};

class ProcessMemoryInfo {
public:
    [[nodiscard]] static std::expected<ProcessMemoryInfo, ProcessInfoValidationError> create(
        std::uint32_t pid,
        std::string name,
        std::uint64_t resident_memory_bytes);

    [[nodiscard]] std::uint32_t pid() const noexcept;
    [[nodiscard]] std::string_view name() const noexcept;
    [[nodiscard]] std::uint64_t resident_memory_bytes() const noexcept;

private:
    ProcessMemoryInfo(
        std::uint32_t pid,
        std::string name,
        std::uint64_t resident_memory_bytes) noexcept;

    std::uint32_t pid_;
    std::string name_;
    std::uint64_t resident_memory_bytes_;
};

}  // namespace memoptima
