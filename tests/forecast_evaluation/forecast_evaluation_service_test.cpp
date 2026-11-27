#include <memoptima/forecast_evaluation/forecast_evaluation_service.hpp>
#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace memoptima::forecast_evaluation {
namespace {

class ForecastEvaluationServiceTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-forecast-evaluation-" + std::to_string(::getpid()));
        std::filesystem::remove_all(directory_);
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }
    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }
    [[nodiscard]] DockerContainerMemorySnapshot snapshot(std::uint64_t bytes) const {
        const auto id = DockerContainerId::parse(std::string(64U, 'a'));
        EXPECT_TRUE(id.has_value());
        return {
            .container_id = *id,
            .cgroup_path = "/docker/evaluation",
            .memory = {.current_memory = CgroupByteMetric{bytes},
                       .limits = {.high = CgroupMemoryLimitMetric{std::optional<std::uint64_t>{}},
                                  .maximum = CgroupMemoryLimitMetric{4'000U}},
                       .events = {.high = CgroupEventMetric{0U},
                                  .maximum = CgroupEventMetric{0U},
                                  .oom = CgroupEventMetric{0U},
                                  .oom_kill = CgroupEventMetric{0U}},
                       .composition = {.anonymous = CgroupByteMetric{0U},
                                       .file_backed = CgroupByteMetric{0U}},
                       .current_swap = CgroupByteMetric{0U},
                       .pressure = {}}};
    }
    [[nodiscard]] std::filesystem::path write_artifact() const {
        const auto count = sampling::SamplingCount::create(20U);
        const auto interval = sampling::SamplingInterval::create(std::chrono::seconds{10});
        EXPECT_TRUE(count.has_value());
        EXPECT_TRUE(interval.has_value());
        const auto path = directory_ / "evaluation.jsonl";
        std::ofstream output{path};
        EXPECT_TRUE(output.is_open());
        output << *sampling::serialize_session_start(*count, *interval, {}) << '\n';
        for (std::size_t index{}; index < 20U; ++index) {
            const auto sequence = sampling::SamplingCount::create(index + 1U);
            EXPECT_TRUE(sequence.has_value());
            std::vector<sampling::DockerContainerEvidence> containers;
            containers.push_back(
                sampling::DockerContainerEvidence::create(snapshot(100U + 10U * index)));
            const auto sample = sampling::SamplingEvidenceSample::create(
                *sequence, std::chrono::seconds{10} * index,
                std::chrono::system_clock::time_point{} + std::chrono::seconds{10} * index, {},
                std::move(containers), 0U, {});
            output << *sampling::serialize_sample(sample) << '\n';
        }
        output << *sampling::serialize_session_end(sampling::SamplingSessionOutcome::completed, 20U,
                                                   std::chrono::system_clock::time_point{} +
                                                       std::chrono::seconds{200},
                                                   std::nullopt)
               << '\n';
        return path;
    }
    std::filesystem::path directory_;
};

TEST_F(ForecastEvaluationServiceTest, ReplaysEvidenceIntoRollingOriginHorizonRows) {
    const auto artifact = write_artifact();
    const auto report =
        evaluate_forecast_manifest({.directory = directory_, .artifact_paths = {artifact}});
    ASSERT_TRUE(report.has_value()) << report.error().detail;
    ASSERT_EQ(report->rows.size(), 4U);
    const auto evaluated =
        std::find_if(report->rows.begin(), report->rows.end(), [](const auto &row) {
            return row.horizon == std::chrono::minutes{1} &&
                   row.status == ForecastEvaluationStatus::evaluated;
        });
    ASSERT_NE(evaluated, report->rows.end());
    EXPECT_GE(evaluated->origin_count, 3U);
    EXPECT_DOUBLE_EQ(*evaluated->mean_absolute_error_bytes, 0.0);
}
} // namespace
} // namespace memoptima::forecast_evaluation
