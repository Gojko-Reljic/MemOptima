#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>
#include <memoptima/trends/trend_analysis_service.hpp>

#include "trends/trend_analysis_replay.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace memoptima::trends {
namespace {

class TrendAnalysisServiceTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-trend-service-" + std::to_string(::getpid()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] DockerContainerMemorySnapshot snapshot(std::string id, std::string cgroup_path,
                                                         std::uint64_t current_bytes) {
        const auto parsed_id = DockerContainerId::parse(std::move(id));
        EXPECT_TRUE(parsed_id.has_value());
        return {
            .container_id = *parsed_id,
            .cgroup_path = std::move(cgroup_path),
            .memory = {
                .current_memory = CgroupByteMetric{current_bytes},
                .limits = {.high = CgroupMemoryLimitMetric{std::optional<std::uint64_t>{}},
                           .maximum = CgroupMemoryLimitMetric{400}},
                .events = {.high = CgroupEventMetric{0},
                           .maximum = CgroupEventMetric{0},
                           .oom = CgroupEventMetric{0},
                           .oom_kill = CgroupEventMetric{0}},
                .composition = {.anonymous = CgroupByteMetric{0},
                                .file_backed = CgroupByteMetric{0}},
                .current_swap = CgroupByteMetric{0},
                .pressure = {
                    .some = CgroupMetric<CgroupPressureLine>{std::unexpected(
                        CgroupMetricReadError{.code = CgroupMetricReadErrorCode::file_read_failed,
                                              .source_name = "memory.pressure",
                                              .detail = "Unavailable in fixture."})},
                    .full = CgroupMetric<CgroupPressureLine>{std::unexpected(
                        CgroupMetricReadError{.code = CgroupMetricReadErrorCode::file_read_failed,
                                              .source_name = "memory.pressure",
                                              .detail = "Unavailable in fixture."})}}}};
    }

    std::filesystem::path
    write_artifact(std::string filename, std::chrono::seconds start,
                   std::vector<std::vector<DockerContainerMemorySnapshot>> sample_containers,
                   bool incomplete) {
        const auto count = sampling::SamplingCount::create(sample_containers.size());
        const auto interval = sampling::SamplingInterval::create(std::chrono::seconds{10});
        EXPECT_TRUE(count.has_value());
        EXPECT_TRUE(interval.has_value());
        const auto path = directory_ / std::move(filename);
        std::ofstream output{path};
        EXPECT_TRUE(output.is_open());
        output << *sampling::serialize_session_start(*count, *interval,
                                                     std::chrono::system_clock::time_point{start})
               << '\n';
        for (std::size_t index{}; index < sample_containers.size(); ++index) {
            std::vector<sampling::DockerContainerEvidence> containers;
            for (auto &container : sample_containers[index]) {
                containers.push_back(
                    sampling::DockerContainerEvidence::create(std::move(container)));
            }
            const auto sequence = sampling::SamplingCount::create(index + 1U);
            EXPECT_TRUE(sequence.has_value());
            const auto sample = sampling::SamplingEvidenceSample::create(
                *sequence, std::chrono::seconds{10} * index,
                std::chrono::system_clock::time_point{start} + std::chrono::seconds{10} * index, {},
                std::move(containers), 0, {});
            output << *sampling::serialize_sample(sample) << '\n';
        }
        if (!incomplete) {
            output << *sampling::serialize_session_end(
                          sampling::SamplingSessionOutcome::completed, sample_containers.size(),
                          std::chrono::system_clock::time_point{start} +
                              std::chrono::seconds{10} * sample_containers.size(),
                          std::nullopt)
                   << '\n';
        }
        return path;
    }

    std::filesystem::path directory_;
};

[[nodiscard]] TrendEvidenceSession catalog_session() {
    return {
        .artifact_path = "session.jsonl",
        .started_at_utc = "1970-01-01T00:00:00.000Z",
        .requested_interval_ms = 10'000U,
        .incomplete = false,
        .container_windows = {{.identity = {.container_id = "container-a", .cgroup_path = "/a"},
                               .minimum_captured_at_utc = "1970-01-01T00:00:00.000Z",
                               .maximum_captured_at_utc = "1970-01-01T00:00:10.000Z"}},
    };
}

[[nodiscard]] detail::TrendReplaySessionMetadata replay_metadata() {
    return {
        .started_at_utc = "1970-01-01T00:00:00.000Z",
        .requested_interval_ms = 10'000U,
        .incomplete = false,
        .container_windows = {{.identity = {.container_id = "container-a", .cgroup_path = "/a"},
                               .minimum_captured_at_utc = "1970-01-01T00:00:00.000Z",
                               .maximum_captured_at_utc = "1970-01-01T00:00:10.000Z"}},
    };
}

TEST_F(TrendAnalysisServiceTest, ReplaysCompleteAndIncompleteCatalogSessions) {
    constexpr std::string_view complete_id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    constexpr std::string_view incomplete_id{
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
    const auto complete =
        write_artifact("complete.jsonl", std::chrono::seconds{0},
                       {{snapshot(std::string{complete_id}, "/docker/complete", 100)},
                        {snapshot(std::string{complete_id}, "/docker/complete", 200)},
                        {snapshot(std::string{complete_id}, "/docker/complete", 300)}},
                       false);
    const auto incomplete =
        write_artifact("incomplete.jsonl", std::chrono::seconds{40},
                       {{snapshot(std::string{incomplete_id}, "/docker/incomplete", 100)}}, true);

    const auto analysis =
        analyze_trend_manifest({.directory = directory_, .artifact_paths = {complete, incomplete}});

    ASSERT_TRUE(analysis.has_value()) << analysis.error().detail;
    EXPECT_EQ(analysis->summary.epoch_count, 2U);
    EXPECT_EQ(analysis->summary.incomplete_epoch_count, 1U);
    EXPECT_EQ(analysis->summary.rate_available_epoch_count, 1U);
}

TEST_F(TrendAnalysisServiceTest, MapsInvalidCatalogArtifactToCatalogFailure) {
    const auto invalid = directory_ / "invalid.jsonl";
    std::ofstream output{invalid};
    ASSERT_TRUE(output.is_open());
    output << "not evidence\n";
    output.close();

    const auto analysis =
        analyze_trend_manifest({.directory = directory_, .artifact_paths = {invalid}});

    ASSERT_FALSE(analysis.has_value());
    EXPECT_EQ(analysis.error().code, TrendAnalysisErrorCode::catalog_failed);
    EXPECT_EQ(analysis.error().artifact_path, invalid);
}

TEST(TrendAnalysisReplayMetadata, AcceptsEquivalentCatalogMetadata) {
    EXPECT_FALSE(
        detail::replay_metadata_mismatch(catalog_session(), replay_metadata()).has_value());
}

TEST(TrendAnalysisReplayMetadata, RejectsChangedSessionMetadata) {
    auto replay = replay_metadata();
    replay.started_at_utc = "1970-01-01T00:00:01.000Z";
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());

    replay = replay_metadata();
    replay.requested_interval_ms = 20'000U;
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());

    replay = replay_metadata();
    replay.incomplete = true;
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());
}

TEST(TrendAnalysisReplayMetadata, RejectsChangedIdentityAndWindowBounds) {
    auto replay = replay_metadata();
    replay.container_windows[0].identity.cgroup_path = "/different";
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());

    replay = replay_metadata();
    replay.container_windows[0].minimum_captured_at_utc = "1969-12-31T23:59:59.000Z";
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());

    replay = replay_metadata();
    replay.container_windows[0].maximum_captured_at_utc = "1970-01-01T00:00:20.000Z";
    EXPECT_TRUE(detail::replay_metadata_mismatch(catalog_session(), replay).has_value());
}

} // namespace
} // namespace memoptima::trends
