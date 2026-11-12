#include <memoptima/sampling/session_runner.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

namespace memoptima::sampling {
namespace {

using Json = nlohmann::json;

class SamplingSessionRunnerTest : public testing::Test {
  protected:
    void SetUp() override {
        const std::string unique_suffix =
            std::to_string(::getpid()) + "-" + std::to_string(next_directory_number_++);
        directory_ =
            std::filesystem::temp_directory_path() / ("memoptima-session-" + unique_suffix);
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] std::filesystem::path output_path() const {
        return directory_ / "evidence.jsonl";
    }

  private:
    inline static std::uint64_t next_directory_number_{};
    std::filesystem::path directory_;
};

[[nodiscard]] DockerContainerScanResult empty_scan_result() {
    return DockerContainerScanResult::create({}, 0, {});
}

[[nodiscard]] SamplingSessionConfiguration configuration(const std::filesystem::path &output,
                                                         std::uint64_t count,
                                                         std::chrono::milliseconds interval) {
    const auto sampling_count = SamplingCount::create(count);
    const auto sampling_interval = SamplingInterval::create(interval);
    EXPECT_TRUE(sampling_count.has_value());
    EXPECT_TRUE(sampling_interval.has_value());
    return SamplingSessionConfiguration{
        .count = *sampling_count,
        .interval = *sampling_interval,
        .output_path = output,
    };
}

[[nodiscard]] std::vector<Json> read_records(const std::filesystem::path &path) {
    std::ifstream input{path};
    std::vector<Json> records;
    std::string line;
    while (std::getline(input, line)) {
        records.push_back(Json::parse(line));
    }
    return records;
}

TEST_F(SamplingSessionRunnerTest, WritesImmediateFixedRateSamplesAndACompletedTerminalRecord) {
    auto steady_now = std::chrono::steady_clock::time_point{};
    const auto epoch = std::chrono::system_clock::time_point{};
    std::uint32_t scan_count{};
    const SamplingSessionDependencies dependencies{
        .preflight = [] { return std::expected<void, std::string>{}; },
        .scan_containers =
            [&] {
                ++scan_count;
                return empty_scan_result();
            },
        .current_utc_time =
            [&] {
                return epoch + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                   steady_now.time_since_epoch());
            },
        .current_steady_time = [&] { return steady_now; },
        .sleep_for = [&](std::chrono::milliseconds duration) { steady_now += duration; },
        .stop_requested = [] { return false; },
    };

    const auto result = run_sampling_session(
        configuration(output_path(), 3, std::chrono::milliseconds{100}), dependencies);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, SamplingSessionOutcome::completed);
    EXPECT_EQ(result->completed_sample_count, 3U);
    EXPECT_FALSE(result->failure.has_value());
    EXPECT_EQ(scan_count, 3U);

    const std::vector<Json> records = read_records(output_path());
    ASSERT_EQ(records.size(), 5U);
    EXPECT_EQ(records.at(0).at("record_type"), "session_start");
    EXPECT_EQ(records.at(1).at("planned_offset_ms"), "0");
    EXPECT_EQ(records.at(1).at("schedule_delay_ms"), "0");
    EXPECT_EQ(records.at(2).at("planned_offset_ms"), "100");
    EXPECT_EQ(records.at(2).at("schedule_delay_ms"), "0");
    EXPECT_EQ(records.at(3).at("planned_offset_ms"), "200");
    EXPECT_EQ(records.at(4).at("record_type"), "session_end");
    EXPECT_EQ(records.at(4).at("outcome"), "completed");
}

TEST_F(SamplingSessionRunnerTest, RecordsCollectionDelayAndWritesAnInterruptedTerminalRecord) {
    auto steady_now = std::chrono::steady_clock::time_point{};
    const auto epoch = std::chrono::system_clock::time_point{};
    std::uint32_t scan_count{};
    const SamplingSessionDependencies dependencies{
        .preflight = [] { return std::expected<void, std::string>{}; },
        .scan_containers =
            [&] {
                ++scan_count;
                steady_now += std::chrono::milliseconds{130};
                return empty_scan_result();
            },
        .current_utc_time =
            [&] {
                return epoch + std::chrono::duration_cast<std::chrono::system_clock::duration>(
                                   steady_now.time_since_epoch());
            },
        .current_steady_time = [&] { return steady_now; },
        .sleep_for = [&](std::chrono::milliseconds duration) { steady_now += duration; },
        .stop_requested = [&] { return scan_count >= 2; },
    };

    const auto result = run_sampling_session(
        configuration(output_path(), 3, std::chrono::milliseconds{100}), dependencies);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->outcome, SamplingSessionOutcome::interrupted);
    EXPECT_EQ(result->completed_sample_count, 2U);
    EXPECT_EQ(scan_count, 2U);

    const std::vector<Json> records = read_records(output_path());
    ASSERT_EQ(records.size(), 4U);
    EXPECT_EQ(records.at(2).at("planned_offset_ms"), "100");
    EXPECT_EQ(records.at(2).at("schedule_delay_ms"), "30");
    EXPECT_EQ(records.at(3).at("outcome"), "interrupted");
    EXPECT_EQ(records.at(3).at("completed_sample_count"), 2);
}

TEST_F(SamplingSessionRunnerTest, PerformsPreflightBeforeCreatingAnArtifact) {
    const SamplingSessionDependencies dependencies{
        .preflight = [] -> std::expected<void, std::string> {
            return std::unexpected("The cgroup v2 memory controller is unavailable.");
        },
        .scan_containers = [] { return empty_scan_result(); },
        .current_utc_time = [] { return std::chrono::system_clock::time_point{}; },
        .current_steady_time = [] { return std::chrono::steady_clock::time_point{}; },
        .sleep_for = [](std::chrono::milliseconds) {},
        .stop_requested = [] { return false; },
    };

    const std::filesystem::path output = output_path();
    const auto result = run_sampling_session(
        configuration(output, 1, std::chrono::milliseconds{100}), dependencies);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, SamplingSessionRunErrorCode::preflight_failed);
    EXPECT_FALSE(std::filesystem::exists(output));
}

} // namespace
} // namespace memoptima::sampling
