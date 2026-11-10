#include "cli/processes_formatter.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace memoptima::cli {
namespace {

[[nodiscard]] ProcessMemoryInfo make_process(std::uint32_t pid, std::string name,
                                             std::uint64_t resident_memory_bytes) {
    const auto process = ProcessMemoryInfo::create(pid, std::move(name), resident_memory_bytes);
    EXPECT_TRUE(process.has_value());
    return *process;
}

TEST(ProcessesFormatter, FormatsProcessTable) {
    linux::ProcessScanResult result{
        .processes =
            {
                make_process(12, "alpha", 1024ULL * 1024ULL * 1024ULL),
                make_process(34, "beta", 512ULL * 1024ULL * 1024ULL),
            },
        .skipped_process_count = 0,
    };

    const std::string output = format_process_scan_result(result);

    EXPECT_NE(output.find("Top memory processes (VmRSS)"), std::string::npos);
    EXPECT_NE(output.find("PID"), std::string::npos);
    EXPECT_NE(output.find("alpha"), std::string::npos);
    EXPECT_NE(output.find("1.00 GiB"), std::string::npos);
}

TEST(ProcessesFormatter, ReportsEmptyResultsAndSkippedProcesses) {
    const linux::ProcessScanResult result{
        .processes = {},
        .skipped_process_count = 2,
    };

    const std::string output = format_process_scan_result(result);

    EXPECT_NE(output.find("No readable process memory records were found."), std::string::npos);
    EXPECT_NE(output.find("2 processes were skipped"), std::string::npos);
}

} // namespace
} // namespace memoptima::cli
