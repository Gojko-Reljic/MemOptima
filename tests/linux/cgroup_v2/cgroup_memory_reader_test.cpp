#include "linux/cgroup_v2/cgroup_memory_reader_impl.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace memoptima::linux::detail {
namespace {

class CgroupMemoryReaderTest : public testing::Test {
  protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() /
                ("memoptima-cgroup-reader-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_ / "container");
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    void write_file(const std::string &name, const std::string &contents) {
        std::ofstream output{root_ / "container" / name};
        output << contents;
    }

    void write_valid_files() {
        write_file("memory.current", "100\n");
        write_file("memory.high", "max\n");
        write_file("memory.max", "200\n");
        write_file("memory.events", "high 1\nmax 2\noom 3\noom_kill 4\n");
        write_file("memory.stat", "anon 60\nfile 40\n");
        write_file("memory.swap.current", "5\n");
        write_file(
            "memory.pressure",
            "some avg10=1 avg60=2 avg300=3 total=4\nfull avg10=0 avg60=0 avg300=0 total=0\n");
    }

    std::filesystem::path root_;
};

TEST_F(CgroupMemoryReaderTest, ReadsACompleteSnapshot) {
    write_valid_files();
    const CgroupMemorySnapshot snapshot = read_cgroup_memory_snapshot_from_root(root_, "container");
    ASSERT_TRUE(snapshot.current_memory().has_value());
    EXPECT_EQ(*snapshot.current_memory(), 100U);
    ASSERT_TRUE(snapshot.limits().high.has_value());
    EXPECT_FALSE(snapshot.limits().high->has_value());
    ASSERT_TRUE(snapshot.events().oom_kill.has_value());
    EXPECT_EQ(*snapshot.events().oom_kill, 4U);
}

TEST_F(CgroupMemoryReaderTest, PreservesOtherMetricsWhenOneFileIsUnavailable) {
    write_valid_files();
    std::filesystem::remove(root_ / "container" / "memory.swap.current");
    const CgroupMemorySnapshot snapshot = read_cgroup_memory_snapshot_from_root(root_, "container");
    EXPECT_TRUE(snapshot.current_memory().has_value());
    ASSERT_FALSE(snapshot.current_swap().has_value());
    EXPECT_EQ(snapshot.current_swap().error().code, CgroupMetricReadErrorCode::file_open_failed);
}

TEST_F(CgroupMemoryReaderTest, RejectsUnsafeRelativePathsWithoutReadingFiles) {
    const CgroupMemorySnapshot snapshot =
        read_cgroup_memory_snapshot_from_root(root_, "../container");
    ASSERT_FALSE(snapshot.current_memory().has_value());
    EXPECT_EQ(snapshot.current_memory().error().code,
              CgroupMetricReadErrorCode::invalid_cgroup_path);
}

} // namespace
} // namespace memoptima::linux::detail
