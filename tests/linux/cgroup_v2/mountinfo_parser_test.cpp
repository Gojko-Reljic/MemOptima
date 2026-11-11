#include "linux/cgroup_v2/mountinfo_parser.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string_view>

namespace memoptima::linux::detail {
namespace {

TEST(MountInfoParser, FindsTheUniqueUnifiedCgroupV2Root) {
    constexpr std::string_view mount_info =
        R"(21 29 0:27 / /sys rw,nosuid,nodev,noexec,relatime - sysfs sysfs rw
32 21 0:28 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw
)";

    const auto result = parse_cgroup_v2_mount_point(mount_info);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{"/sys/fs/cgroup"});
}

TEST(MountInfoParser, DecodesEscapedMountPointCharacters) {
    constexpr std::string_view mount_info =
        "32 21 0:28 / /custom\\040cgroup rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw\n";

    const auto result = parse_cgroup_v2_mount_point(mount_info);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{"/custom cgroup"});
}

TEST(MountInfoParser, IgnoresCgroupV2BindMounts) {
    constexpr std::string_view mount_info =
        R"(32 21 0:28 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw
33 21 0:28 /docker /run/cgroup-bind rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw
)";

    const auto result = parse_cgroup_v2_mount_point(mount_info);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, std::filesystem::path{"/sys/fs/cgroup"});
}

TEST(MountInfoParser, RejectsMissingSeparator) {
    const auto result =
        parse_cgroup_v2_mount_point("32 21 0:28 / /sys/fs/cgroup cgroup2 cgroup rw\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MountInfoParseErrorCode::invalid_format);
}

TEST(MountInfoParser, RejectsInvalidEscapeSequence) {
    const auto result = parse_cgroup_v2_mount_point(
        "32 21 0:28 / /custom\\04 rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MountInfoParseErrorCode::invalid_escape_sequence);
}

TEST(MountInfoParser, RejectsWhenNoUnifiedCgroupV2RootExists) {
    const auto result = parse_cgroup_v2_mount_point(
        "32 21 0:28 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime - tmpfs tmpfs rw\n");

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MountInfoParseErrorCode::cgroup_v2_not_found);
}

TEST(MountInfoParser, RejectsAmbiguousUnifiedCgroupV2Roots) {
    constexpr std::string_view mount_info =
        R"(32 21 0:28 / /sys/fs/cgroup rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw
33 21 0:29 / /alternate/cgroup rw,nosuid,nodev,noexec,relatime - cgroup2 cgroup rw
)";

    const auto result = parse_cgroup_v2_mount_point(mount_info);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, MountInfoParseErrorCode::ambiguous_cgroup_v2_mount);
}

} // namespace
} // namespace memoptima::linux::detail
