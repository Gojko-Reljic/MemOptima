#include <memoptima/forecast_evaluation/forecast_evaluation_service.hpp>
#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace memoptima::forecast_evaluation {
namespace {

class ForecastEvaluationServiceTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-forecast-evaluation-service-" + std::to_string(::getpid()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    [[nodiscard]] DockerContainerMemorySnapshot snapshot(std::uint64_t current_bytes) {
        const auto parsed = DockerContainerId::parse(std::string(64U, 'a'));
        EXPECT_TRUE(parsed.has_value());
        return {
            .container_id = *parsed,
            .cgroup_path = "/docker/evaluation",
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

    std::filesystem::path write_artifact(std::string filename,
                                         std::vector<std::uint64_t> current_bytes,
                                         bool incomplete = false) {
        const auto count = sampling::SamplingCount::create(current_bytes.size());
        const auto interval = sampling::SamplingInterval::create(std::chrono::seconds{5});
        EXPECT_TRUE(count.has_value());
        EXPECT_TRUE(interval.has_value());
        const auto path = directory_ / std::move(filename);
        std::ofstream output{path};
        EXPECT_TRUE(output.is_open());
        output << *sampling::serialize_session_start(*count, *interval,
                                                     std::chrono::system_clock::time_point{})
               << '\n';
        for (std::size_t index{}; index < current_bytes.size(); ++index) {
            const auto sequence = sampling::SamplingCount::create(index + 1U);
            EXPECT_TRUE(sequence.has_value());
            std::vector<sampling::DockerContainerEvidence> containers;
            containers.push_back(
                sampling::DockerContainerEvidence::create(snapshot(current_bytes[index])));
            const auto sample = sampling::SamplingEvidenceSample::create(
                *sequence, std::chrono::seconds{5} * index,
                std::chrono::system_clock::time_point{} + std::chrono::seconds{5} * index, {},
                std::move(containers), 0U, {});
            output << *sampling::serialize_sample(sample) << '\n';
        }
        if (!incomplete) {
            output << *sampling::serialize_session_end(
                          sampling::SamplingSessionOutcome::completed, current_bytes.size(),
                          std::chrono::system_clock::time_point{} +
                              std::chrono::seconds{5} * current_bytes.size(),
                          std::nullopt)
                   << '\n';
        }
        return path;
    }

    std::filesystem::path directory_;
};

TEST_F(ForecastEvaluationServiceTest, ReplaysManifestEvidenceIntoACompleteEvaluation) {
    const auto artifact = write_artifact("evaluation.jsonl", {100U, 110U, 120U, 130U});
    const auto report =
        evaluate_forecast_manifest({.directory = directory_, .artifact_paths = {artifact}});

    ASSERT_TRUE(report.has_value()) << report.error().detail;
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows.front().status, ForecastEvaluationStatus::evaluated);
    EXPECT_EQ(report->rows.front().actual_current_bytes, 130U);
    EXPECT_EQ(report->summary.evaluated_forecast_count, 1U);
}

TEST_F(ForecastEvaluationServiceTest, MapsInvalidCatalogInputToCatalogFailure) {
    const auto artifact = directory_ / "invalid.jsonl";
    std::ofstream output{artifact};
    ASSERT_TRUE(output.is_open());
    output << "not evidence\n";
    output.close();

    const auto report =
        evaluate_forecast_manifest({.directory = directory_, .artifact_paths = {artifact}});

    ASSERT_FALSE(report.has_value());
    EXPECT_EQ(report.error().code, ForecastEvaluationErrorCode::catalog_failed);
    EXPECT_EQ(report.error().artifact_path, artifact);
}

TEST_F(ForecastEvaluationServiceTest, PreservesIncompleteEvidenceAsAnExcludedRow) {
    const auto artifact = write_artifact("incomplete.jsonl", {100U, 110U, 120U, 130U}, true);
    const auto report =
        evaluate_forecast_manifest({.directory = directory_, .artifact_paths = {artifact}});

    ASSERT_TRUE(report.has_value()) << report.error().detail;
    ASSERT_EQ(report->rows.size(), 1U);
    EXPECT_EQ(report->rows.front().status, ForecastEvaluationStatus::excluded_incomplete_session);
    EXPECT_EQ(report->summary.excluded_incomplete_session_count, 1U);
}

} // namespace
} // namespace memoptima::forecast_evaluation
