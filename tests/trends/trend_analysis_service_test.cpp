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

[[nodiscard]] std::string container_id_for(std::size_t index) {
    constexpr std::string_view hexadecimal = "0123456789abcdef";
    std::string id(64U, 'a');
    for (std::size_t digit{}; digit < 4U; ++digit) {
        const auto shift = (3U - digit) * 4U;
        id[60U + digit] = hexadecimal[(index >> shift) & 0xFU];
    }
    return id;
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

TEST_F(TrendAnalysisServiceTest, MapsAnOversizedDirectManifestToCatalogFailure) {
    TrendManifest manifest{.directory = directory_, .artifact_paths = {}};
    manifest.artifact_paths.resize(kTrendManifestMaximumArtifacts + 1U,
                                   directory_ / "unread-artifact.jsonl");

    const auto analysis = analyze_trend_manifest(manifest);

    ASSERT_FALSE(analysis.has_value());
    EXPECT_EQ(analysis.error().code, TrendAnalysisErrorCode::catalog_failed);
}

TEST_F(TrendAnalysisServiceTest, MapsCatalogReplayMismatchToReplayFailure) {
    constexpr std::string_view id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    const auto artifact =
        write_artifact("mismatch.jsonl", std::chrono::seconds{0},
                       {{snapshot(std::string{id}, "/docker/mismatch", 100)}}, false);
    TrendAnalyzer analyzer;
    const TrendEvidenceSession cataloged{
        .artifact_path = artifact,
        .started_at_utc = "1970-01-01T00:00:01.000Z",
        .requested_interval_ms = 10'000U,
        .incomplete = false,
        .container_windows = {},
    };

    const auto replayed = detail::replay_trend_evidence_session(cataloged, analyzer);

    ASSERT_FALSE(replayed.has_value());
    EXPECT_EQ(replayed.error().code, TrendAnalysisErrorCode::evidence_replay_failed);
    EXPECT_EQ(replayed.error().artifact_path, artifact);
}

TEST_F(TrendAnalysisServiceTest, MapsAnalyzerResourceLimitToAnalyzerFailure) {
    constexpr std::string_view id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    const TrendContainerIdentity identity{.container_id = std::string{id},
                                          .cgroup_path = "/docker/limit"};
    TrendAnalyzer analyzer;
    for (std::uint64_t index{}; index < 4096U; ++index) {
        ASSERT_TRUE(analyzer
                        .begin_session({.started_at = TrendTimePoint{std::chrono::milliseconds{
                                            static_cast<std::int64_t>(index)}},
                                        .requested_interval_ms = 1U})
                        .has_value());
        ASSERT_TRUE(analyzer
                        .begin_sample({.captured_at = TrendTimePoint{std::chrono::milliseconds{
                                           static_cast<std::int64_t>(index)}}})
                        .has_value());
        ASSERT_TRUE(analyzer
                        .observe({.identity = identity,
                                  .current_memory_bytes = 1U,
                                  .maximum_memory = {.kind = TrendMemoryMaximumKind::finite,
                                                     .bytes = index}})
                        .has_value());
        ASSERT_TRUE(analyzer.end_sample().has_value());
        ASSERT_TRUE(analyzer.end_session({.incomplete = false}).has_value());
    }

    const auto artifact = write_artifact("analyzer-limit.jsonl", std::chrono::seconds{10},
                                         {{snapshot(std::string{id}, "/docker/limit", 1)}}, false);
    const TrendEvidenceSession cataloged{
        .artifact_path = artifact,
        .started_at_utc = "1970-01-01T00:00:10.000Z",
        .requested_interval_ms = 10'000U,
        .incomplete = false,
        .container_windows = {{.identity = identity,
                               .minimum_captured_at_utc = "1970-01-01T00:00:10.000Z",
                               .maximum_captured_at_utc = "1970-01-01T00:00:10.000Z"}},
    };

    const auto replayed = detail::replay_trend_evidence_session(cataloged, analyzer);

    ASSERT_FALSE(replayed.has_value());
    EXPECT_EQ(replayed.error().code, TrendAnalysisErrorCode::analyzer_failed);
    EXPECT_EQ(replayed.error().artifact_path, artifact);
}

TEST_F(TrendAnalysisServiceTest, BoundsNewReplayIdentityWindowsBeforeAllocation) {
    std::vector<DockerContainerMemorySnapshot> containers;
    containers.reserve(kTrendEvidenceCatalogMaximumContainerWindows + 1U);
    for (std::size_t index{}; index <= kTrendEvidenceCatalogMaximumContainerWindows; ++index) {
        containers.push_back(snapshot(container_id_for(index), "/docker/replay-bound", 1U));
    }
    const auto artifact = write_artifact("replay-bound.jsonl", std::chrono::seconds{0},
                                         {std::move(containers)}, false);
    TrendAnalyzer analyzer;
    const TrendEvidenceSession cataloged{
        .artifact_path = artifact,
        .started_at_utc = "1970-01-01T00:00:00.000Z",
        .requested_interval_ms = 10'000U,
        .incomplete = false,
        .container_windows = {},
    };

    const auto replayed = detail::replay_trend_evidence_session(cataloged, analyzer);

    ASSERT_FALSE(replayed.has_value());
    EXPECT_EQ(replayed.error().code, TrendAnalysisErrorCode::evidence_replay_failed);
    EXPECT_EQ(replayed.error().artifact_path, artifact);
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
