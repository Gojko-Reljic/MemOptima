#include "linux/cgroup_v2/docker_container_scanner_impl.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace memoptima::linux::detail {
namespace {

class DockerContainerScannerTest : public testing::Test {
  protected:
    void SetUp() override {
        root_ = std::filesystem::temp_directory_path() /
                ("memoptima-docker-scanner-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root_);
    }

    void TearDown() override { std::filesystem::remove_all(root_); }

    [[nodiscard]] static std::string container_id(char character) {
        return std::string(64, character);
    }

    void create_cgroup(const std::filesystem::path &relative_path, std::uint64_t current_memory) {
        const std::filesystem::path directory = root_ / relative_path;
        std::filesystem::create_directories(directory);
        write_file(directory, "memory.current", std::to_string(current_memory) + '\n');
        write_file(directory, "memory.high", "max\n");
        write_file(directory, "memory.max", "max\n");
        write_file(directory, "memory.events", "high 0\nmax 0\noom 0\noom_kill 0\n");
        write_file(directory, "memory.stat", "anon 0\nfile 0\n");
        write_file(directory, "memory.swap.current", "0\n");
        write_file(directory, "memory.pressure",
                   "some avg10=0 avg60=0 avg300=0 total=0\n"
                   "full avg10=0 avg60=0 avg300=0 total=0\n");
    }

  protected:
    static void write_file(const std::filesystem::path &directory, const std::string &name,
                           const std::string &contents) {
        std::ofstream output{directory / name};
        output << contents;
    }

    std::filesystem::path root_;
};

TEST_F(DockerContainerScannerTest, ReportsMissingStandardLayoutsWithoutFailingTheScan) {
    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    EXPECT_TRUE(result.containers().empty());
    EXPECT_EQ(result.skipped_container_count(), 0U);
    ASSERT_EQ(result.warnings().size(), 1U);
    EXPECT_EQ(result.warnings().front().code,
              DockerContainerScanWarningCode::docker_cgroup_layout_not_found);
}

TEST_F(DockerContainerScannerTest, DiscoversAndSortsCgroupfsContainersByIdentifier) {
    const std::string later_id = container_id('b');
    const std::string earlier_id = container_id('a');
    create_cgroup(std::filesystem::path{"docker"} / later_id, 200);
    create_cgroup(std::filesystem::path{"docker"} / earlier_id, 100);

    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    ASSERT_EQ(result.containers().size(), 2U);
    EXPECT_EQ(result.containers()[0].container_id().value(), earlier_id);
    EXPECT_EQ(result.containers()[1].container_id().value(), later_id);
    ASSERT_TRUE(result.containers()[0].memory().current_memory().has_value());
    EXPECT_EQ(*result.containers()[0].memory().current_memory(), 100U);
    EXPECT_TRUE(result.warnings().empty());
}

TEST_F(DockerContainerScannerTest, DiscoversSystemdContainers) {
    const std::string id = container_id('c');
    const std::filesystem::path cgroup_path =
        std::filesystem::path{"system.slice"} / ("docker-" + id + ".scope");
    create_cgroup(cgroup_path, 300);

    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    ASSERT_EQ(result.containers().size(), 1U);
    EXPECT_EQ(result.containers().front().container_id().value(), id);
    EXPECT_EQ(result.containers().front().cgroup_path(), root_ / cgroup_path);
}

TEST_F(DockerContainerScannerTest, KeepsSystemdSnapshotWhenAnIdentifierIsDuplicated) {
    const std::string id = container_id('d');
    create_cgroup(std::filesystem::path{"docker"} / id, 100);
    const std::filesystem::path systemd_path =
        std::filesystem::path{"system.slice"} / ("docker-" + id + ".scope");
    create_cgroup(systemd_path, 200);

    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    ASSERT_EQ(result.containers().size(), 1U);
    EXPECT_EQ(result.containers().front().cgroup_path(), root_ / systemd_path);
    ASSERT_TRUE(result.containers().front().memory().current_memory().has_value());
    EXPECT_EQ(*result.containers().front().memory().current_memory(), 200U);
    ASSERT_EQ(result.warnings().size(), 1U);
    EXPECT_EQ(result.warnings().front().code,
              DockerContainerScanWarningCode::duplicate_container_identifier);
}

TEST_F(DockerContainerScannerTest, ReportsInvalidContainerIdentifiersAndContinues) {
    create_cgroup(std::filesystem::path{"docker"} / "not-a-container", 100);
    const std::string valid_id = container_id('e');
    create_cgroup(std::filesystem::path{"docker"} / valid_id, 200);

    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    ASSERT_EQ(result.containers().size(), 1U);
    EXPECT_EQ(result.containers().front().container_id().value(), valid_id);
    EXPECT_EQ(result.skipped_container_count(), 1U);
    ASSERT_EQ(result.warnings().size(), 1U);
    EXPECT_EQ(result.warnings().front().code,
              DockerContainerScanWarningCode::invalid_container_identifier);
}

TEST_F(DockerContainerScannerTest, PreservesAContainerWithPartiallyUnavailableMetrics) {
    const std::string id = container_id('f');
    const std::filesystem::path cgroup_path = std::filesystem::path{"docker"} / id;
    create_cgroup(cgroup_path, 100);
    std::filesystem::remove(root_ / cgroup_path / "memory.pressure");

    const DockerContainerScanResult result = scan_rootful_docker_containers_from_root(root_);

    ASSERT_EQ(result.containers().size(), 1U);
    EXPECT_TRUE(result.containers().front().memory().current_memory().has_value());
    EXPECT_FALSE(result.containers().front().memory().pressure().some.has_value());
    EXPECT_TRUE(result.warnings().empty());
}

} // namespace
} // namespace memoptima::linux::detail
