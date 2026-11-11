#include "linux/cgroup_v2/cgroup_memory_parser.hpp"

#include <gtest/gtest.h>

namespace memoptima::linux::detail {
namespace {

TEST(CgroupMemoryParser, ParsesByteValuesAndUnlimitedLimits) {
    const auto bytes = parse_cgroup_byte_value("  4096\n", "memory.current");
    const auto unlimited = parse_cgroup_memory_limit("max\n", "memory.high");
    const auto limited = parse_cgroup_memory_limit("8192\n", "memory.max");

    ASSERT_TRUE(bytes.has_value());
    EXPECT_EQ(*bytes, 4096U);
    ASSERT_TRUE(unlimited.has_value());
    EXPECT_FALSE(unlimited->has_value());
    ASSERT_TRUE(limited.has_value());
    ASSERT_TRUE(limited->has_value());
    EXPECT_EQ(**limited, 8192U);
}

TEST(CgroupMemoryParser, RejectsInvalidAndOverflowingByteValues) {
    const auto invalid = parse_cgroup_byte_value("-1", "memory.current");
    const auto overflow = parse_cgroup_byte_value("18446744073709551616", "memory.current");

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().code, CgroupMetricReadErrorCode::invalid_value);
    ASSERT_FALSE(overflow.has_value());
    EXPECT_EQ(overflow.error().code, CgroupMetricReadErrorCode::value_overflow);
}

TEST(CgroupMemoryParser, ParsesRequiredEventsAndMemoryStatValues) {
    const auto events = parse_cgroup_memory_events("high 1\nmax 2\noom 3\noom_kill 4\n");
    const auto stat = parse_cgroup_memory_stat("anon 1024\nfile 2048\nslab 512\n");

    ASSERT_TRUE(events.has_value());
    EXPECT_EQ(events->oom_kill, 4U);
    ASSERT_TRUE(stat.has_value());
    EXPECT_EQ(stat->anonymous, 1024U);
    EXPECT_EQ(stat->file_backed, 2048U);
}

TEST(CgroupMemoryParser, RejectsMissingAndDuplicateRequiredValues) {
    const auto missing = parse_cgroup_memory_events("high 1\nmax 2\noom 3\n");
    const auto duplicate = parse_cgroup_memory_stat("anon 1\nanon 2\nfile 3\n");

    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, CgroupMetricReadErrorCode::missing_required_value);
    ASSERT_FALSE(duplicate.has_value());
    EXPECT_EQ(duplicate.error().code, CgroupMetricReadErrorCode::duplicate_required_value);
}

} // namespace
} // namespace memoptima::linux::detail
