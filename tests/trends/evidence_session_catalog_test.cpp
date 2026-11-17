#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>
#include <memoptima/trends/evidence_session_catalog.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace memoptima::trends {
namespace {

class TrendEvidenceSessionCatalogTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-trend-catalog-" + std::to_string(::getpid()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] DockerContainerMemorySnapshot snapshot(std::string id, std::string cgroup_path) {
        const auto parsed_id = DockerContainerId::parse(std::move(id));
        EXPECT_TRUE(parsed_id.has_value());
        return {
            .container_id = *parsed_id,
            .cgroup_path = std::move(cgroup_path),
            .memory = {
                .current_memory = CgroupByteMetric{100},
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

    [[nodiscard]] TrendManifest manifest(std::string_view json) const {
        const auto parsed = parse_trend_manifest(json, directory_);
        EXPECT_TRUE(parsed.has_value());
        return *parsed;
    }

    std::filesystem::path directory_;
};

TEST_F(TrendEvidenceSessionCatalogTest, SortsSessionsAndPreservesIncompleteStatus) {
    constexpr std::string_view first_id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    constexpr std::string_view second_id{
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"};
    write_artifact("later.jsonl", std::chrono::seconds{30},
                   {{snapshot(std::string{second_id}, "/docker/second")}}, false);
    write_artifact("earlier.jsonl", std::chrono::seconds{0},
                   {{snapshot(std::string{first_id}, "/docker/first")}}, true);

    const auto catalog = load_trend_evidence_catalog(manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"later.jsonl"},{"path":"earlier.jsonl"}]})"));

    ASSERT_TRUE(catalog.has_value());
    ASSERT_EQ(catalog->sessions.size(), 2U);
    EXPECT_EQ(catalog->sessions[0].artifact_path.filename(), "earlier.jsonl");
    EXPECT_TRUE(catalog->sessions[0].incomplete);
    ASSERT_EQ(catalog->sessions[0].container_windows.size(), 1U);
    EXPECT_EQ(catalog->sessions[0].container_windows[0].identity.container_id, first_id);
    EXPECT_EQ(catalog->sessions[1].artifact_path.filename(), "later.jsonl");
    EXPECT_FALSE(catalog->sessions[1].incomplete);
}

TEST_F(TrendEvidenceSessionCatalogTest, RejectsOverlappingStrictContainerIdentity) {
    constexpr std::string_view id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    write_artifact("first.jsonl", std::chrono::seconds{0},
                   {{snapshot(std::string{id}, "/docker/shared")},
                    {snapshot(std::string{id}, "/docker/shared")}},
                   false);
    write_artifact("second.jsonl", std::chrono::seconds{5},
                   {{snapshot(std::string{id}, "/docker/shared")}}, false);

    const auto catalog = load_trend_evidence_catalog(manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"first.jsonl"},{"path":"second.jsonl"}]})"));

    ASSERT_FALSE(catalog.has_value());
    EXPECT_EQ(catalog.error().code, TrendEvidenceCatalogErrorCode::ambiguous_session_overlap);
    EXPECT_EQ(catalog.error().artifact_path.filename(), "second.jsonl");
}

TEST_F(TrendEvidenceSessionCatalogTest, AllowsTheSameContainerIdAtDifferentCgroupPaths) {
    constexpr std::string_view id{
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};
    write_artifact("first.jsonl", std::chrono::seconds{0},
                   {{snapshot(std::string{id}, "/docker/first")}}, false);
    write_artifact("second.jsonl", std::chrono::seconds{0},
                   {{snapshot(std::string{id}, "/docker/second")}}, false);

    const auto catalog = load_trend_evidence_catalog(manifest(
        R"({"manifest_version":1,"artifacts":[{"path":"first.jsonl"},{"path":"second.jsonl"}]})"));

    ASSERT_TRUE(catalog.has_value());
    EXPECT_EQ(catalog->sessions.size(), 2U);
}

TEST_F(TrendEvidenceSessionCatalogTest, RejectsAnArtifactThatFailsEvidenceValidation) {
    const auto path = directory_ / "invalid.jsonl";
    std::ofstream output{path};
    ASSERT_TRUE(output.is_open());
    output << "this is not JSONL evidence\n";
    output.close();

    const auto catalog = load_trend_evidence_catalog(
        manifest(R"({"manifest_version":1,"artifacts":[{"path":"invalid.jsonl"}]})"));

    ASSERT_FALSE(catalog.has_value());
    EXPECT_EQ(catalog.error().code, TrendEvidenceCatalogErrorCode::artifact_validation_failed);
    EXPECT_EQ(catalog.error().artifact_path.filename(), "invalid.jsonl");
}

} // namespace
} // namespace memoptima::trends
