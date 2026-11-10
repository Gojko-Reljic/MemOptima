#include <memoptima/linux/process_scanner.hpp>

#include "linux/process_ranker.hpp"
#include "linux/process_status_parser.hpp"

#include <charconv>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace memoptima::linux {
namespace {

constexpr std::string_view kProcDirectoryPath{"/proc"};
constexpr std::size_t kDefaultProcessLimit = 10;

[[nodiscard]] ProcessScanError make_scan_error(ProcessScanErrorCode code, std::string_view detail) {
    return {
        .code = code,
        .detail = std::string{detail},
    };
}

[[nodiscard]] std::expected<std::uint32_t, std::monostate> parse_pid(std::string_view text) {
    if (text.empty()) {
        return std::unexpected(std::monostate{});
    }

    std::uint32_t pid{};
    const auto [parse_end, parse_error] = std::from_chars(text.data(), text.data() + text.size(), pid);
    if (parse_error != std::errc{} || parse_end != text.data() + text.size() || pid == 0) {
        return std::unexpected(std::monostate{});
    }

    return pid;
}

[[nodiscard]] std::expected<ProcessMemoryInfo, std::monostate> read_process_memory_info(std::uint32_t pid) {
    const std::filesystem::path status_path =
        std::filesystem::path{kProcDirectoryPath} / std::to_string(pid) / "status";

    std::ifstream status_file{status_path};
    if (!status_file.is_open()) {
        return std::unexpected(std::monostate{});
    }

    const std::string contents{
        std::istreambuf_iterator<char>{status_file},
        std::istreambuf_iterator<char>{}};
    if (status_file.bad()) {
        return std::unexpected(std::monostate{});
    }

    const auto process = detail::parse_process_status(pid, contents);
    if (!process.has_value()) {
        return std::unexpected(std::monostate{});
    }

    return *process;
}

}  // namespace

std::expected<ProcessScanResult, ProcessScanError> scan_process_memory() {
    std::error_code error;
    std::filesystem::directory_iterator iterator{
        std::filesystem::path{kProcDirectoryPath},
        std::filesystem::directory_options::skip_permission_denied,
        error};
    if (error) {
        return std::unexpected(make_scan_error(
            ProcessScanErrorCode::proc_directory_open_failed,
            "Unable to open /proc."));
    }

    std::vector<ProcessMemoryInfo> processes;
    std::size_t skipped_process_count{};

    const std::filesystem::directory_iterator end;
    while (iterator != end) {
        const std::string directory_name = iterator->path().filename().string();
        const auto pid = parse_pid(directory_name);

        if (pid.has_value()) {
            const auto process = read_process_memory_info(*pid);
            if (process.has_value()) {
                processes.push_back(*process);
            } else {
                ++skipped_process_count;
            }
        }

        iterator.increment(error);
        if (error) {
            return std::unexpected(make_scan_error(
                ProcessScanErrorCode::directory_iteration_failed,
                "Unable to continue reading /proc."));
        }
    }

    return ProcessScanResult{
        .processes = detail::rank_processes(std::move(processes), kDefaultProcessLimit),
        .skipped_process_count = skipped_process_count,
    };
}

}  // namespace memoptima::linux
