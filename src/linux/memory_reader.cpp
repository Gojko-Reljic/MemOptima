#include <memoptima/linux/memory_reader.hpp>

#include "linux/meminfo_parser.hpp"

#include <fstream>
#include <iterator>
#include <string>
#include <string_view>

namespace memoptima::linux {
namespace {

constexpr std::string_view kMemInfoPath{"/proc/meminfo"};

[[nodiscard]] MemoryReadError make_file_error(MemoryReadErrorCode code, std::string_view detail) {
    return {
        .code = code,
        .metric_name = {},
        .line_number = 0,
        .detail = std::string{detail},
    };
}

}  // namespace

std::expected<MemorySnapshot, MemoryReadError> read_system_memory_snapshot() {
    std::ifstream memory_info_file{std::string{kMemInfoPath}};
    if (!memory_info_file.is_open()) {
        return std::unexpected(make_file_error(
            MemoryReadErrorCode::file_open_failed,
            "Unable to open /proc/meminfo."));
    }

    const std::string contents{
        std::istreambuf_iterator<char>{memory_info_file},
        std::istreambuf_iterator<char>{}};

    if (memory_info_file.bad()) {
        return std::unexpected(make_file_error(
            MemoryReadErrorCode::file_read_failed,
            "Unable to read /proc/meminfo."));
    }

    return detail::parse_memory_info(contents);
}

}  // namespace memoptima::linux
