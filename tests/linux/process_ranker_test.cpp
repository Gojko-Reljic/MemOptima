#include "linux/process_ranker.hpp"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace memoptima::linux::detail {
namespace {

[[nodiscard]] ProcessMemoryInfo make_process(std::uint32_t pid,
                                             std::uint64_t resident_memory_bytes) {
    const auto process = ProcessMemoryInfo::create(pid, "process", resident_memory_bytes);
    EXPECT_TRUE(process.has_value());
    return *process;
}

TEST(ProcessRanker, SortsByResidentMemoryThenPidAndAppliesLimit) {
    std::vector<ProcessMemoryInfo> processes;
    processes.push_back(make_process(30, 100));
    processes.push_back(make_process(20, 200));
    processes.push_back(make_process(10, 200));
    processes.push_back(make_process(40, 50));

    const std::vector<ProcessMemoryInfo> ranked_processes = rank_processes(std::move(processes), 3);

    ASSERT_EQ(ranked_processes.size(), 3U);
    EXPECT_EQ(ranked_processes[0].pid(), 10U);
    EXPECT_EQ(ranked_processes[1].pid(), 20U);
    EXPECT_EQ(ranked_processes[2].pid(), 30U);
}

} // namespace
} // namespace memoptima::linux::detail
