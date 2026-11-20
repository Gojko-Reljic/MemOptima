#include <memoptima/forecast/evidence_forecast.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <unistd.h>
#include <utility>
#include <vector>

namespace memoptima::forecast {
namespace {

constexpr std::string_view kId{"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"};

CgroupMetricReadError unavailable(std::string source) {
    return {.code = CgroupMetricReadErrorCode::file_read_failed,
            .source_name = std::move(source),
            .detail = "Fixture metric unavailable."};
}

DockerContainerMemorySnapshot snapshot(std::uint64_t current, std::optional<std::uint64_t> maximum,
                                       bool current_available = true,
                                       bool maximum_available = true) {
    const auto id = DockerContainerId::parse(kId);
    EXPECT_TRUE(id.has_value());
    return {
        .container_id = *id,
        .cgroup_path = "/sys/fs/cgroup/docker/a",
        .memory = {
            .current_memory = current_available ? CgroupByteMetric{current}
                                                : CgroupByteMetric{std::unexpected(
                                                      unavailable("memory.current"))},
            .limits = {.high = CgroupMemoryLimitMetric{std::optional<std::uint64_t>{}},
                       .maximum = maximum_available ? CgroupMemoryLimitMetric{maximum}
                                                    : CgroupMemoryLimitMetric{std::unexpected(
                                                          unavailable("memory.max"))}},
            .events = {.high = CgroupEventMetric{0},
                       .maximum = CgroupEventMetric{0},
                       .oom = CgroupEventMetric{0},
                       .oom_kill = CgroupEventMetric{0}},
            .composition = {.anonymous = CgroupByteMetric{0}, .file_backed = CgroupByteMetric{0}},
            .current_swap = CgroupByteMetric{0},
            .pressure = {.some = CgroupMetric<CgroupPressureLine>{std::unexpected(
                             unavailable("memory.pressure"))},
                         .full = CgroupMetric<CgroupPressureLine>{
                             std::unexpected(unavailable("memory.pressure"))}}}};
}

std::filesystem::path
write_artifact(const std::vector<std::optional<DockerContainerMemorySnapshot>> &samples,
               std::chrono::seconds interval_duration = std::chrono::seconds{10}) {
    static std::atomic<unsigned> next_artifact_number{};
    const auto path = std::filesystem::temp_directory_path() /
                      ("memoptima-forecast-" + std::to_string(::getpid()) + "-" +
                       std::to_string(next_artifact_number++) + ".jsonl");
    const auto count = sampling::SamplingCount::create(samples.size());
    const auto interval = sampling::SamplingInterval::create(interval_duration);
    EXPECT_TRUE(count.has_value());
    EXPECT_TRUE(interval.has_value());
    std::ofstream output{path};
    output << *sampling::serialize_session_start(*count, *interval, {}) << '\n';
    for (std::size_t index{}; index < samples.size(); ++index) {
        std::vector<sampling::DockerContainerEvidence> containers;
        if (samples[index].has_value())
            containers.push_back(sampling::DockerContainerEvidence::create(*samples[index]));
        const auto sequence = sampling::SamplingCount::create(index + 1);
        const auto sample = sampling::SamplingEvidenceSample::create(
            *sequence, interval_duration * index,
            std::chrono::system_clock::time_point{interval_duration * index}, {},
            std::move(containers), 0, {});
        output << *sampling::serialize_sample(sample) << '\n';
    }
    output << *sampling::serialize_session_end(
                  sampling::SamplingSessionOutcome::completed, samples.size(),
                  std::chrono::system_clock::time_point{interval_duration * samples.size()},
                  std::nullopt)
           << '\n';
    return path;
}

TEST(EvidenceForecast, ForecastsAndReportsEvidenceAvailability) {
    const auto path = write_artifact({snapshot(100, 400), snapshot(200, 400), snapshot(300, 400)});
    const auto result = forecast_evidence_artifact(path);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->containers.size(), 1U);
    EXPECT_EQ(result->containers[0].status, EvidenceForecastStatus::available);
    EXPECT_EQ(result->containers[0].estimated_time_to_memory_max, std::chrono::seconds{10});
    std::filesystem::remove(path);
}

TEST(EvidenceForecast, RejectsDiscontinuousUnlimitedUnavailableUnstableAndFallingSeries) {
    const auto discontinuous =
        write_artifact({snapshot(100, 400), std::nullopt, snapshot(300, 400)});
    const auto unlimited = write_artifact(
        {snapshot(100, std::nullopt), snapshot(200, std::nullopt), snapshot(300, std::nullopt)});
    const auto unavailable_current =
        write_artifact({snapshot(100, 400, false), snapshot(200, 400), snapshot(300, 400)});
    const auto unstable =
        write_artifact({snapshot(100, 400), snapshot(200, 500), snapshot(300, 400)});
    const auto falling =
        write_artifact({snapshot(300, 400), snapshot(200, 400), snapshot(100, 400)});
    EXPECT_EQ(forecast_evidence_artifact(discontinuous)->containers[0].status,
              EvidenceForecastStatus::unavailable_container_not_observed_continuously);
    EXPECT_EQ(forecast_evidence_artifact(unlimited)->containers[0].status,
              EvidenceForecastStatus::not_applicable_unlimited_memory_max);
    EXPECT_EQ(forecast_evidence_artifact(unavailable_current)->containers[0].status,
              EvidenceForecastStatus::unavailable_current_memory);
    EXPECT_EQ(forecast_evidence_artifact(unstable)->containers[0].status,
              EvidenceForecastStatus::unavailable_unstable_memory_max);
    EXPECT_EQ(forecast_evidence_artifact(falling)->containers[0].status,
              EvidenceForecastStatus::unavailable_non_positive_growth);
    std::filesystem::remove(discontinuous);
    std::filesystem::remove(unlimited);
    std::filesystem::remove(unavailable_current);
    std::filesystem::remove(unstable);
    std::filesystem::remove(falling);
}

TEST(EvidenceForecast, ReportsInsufficientObservationCountAndWindow) {
    const auto insufficient_count = write_artifact({snapshot(100, 400)});
    const auto insufficient_window = write_artifact(
        {snapshot(100, 400), snapshot(200, 400), snapshot(300, 400)}, std::chrono::seconds{1});

    EXPECT_EQ(forecast_evidence_artifact(insufficient_count)->containers[0].status,
              EvidenceForecastStatus::unavailable_insufficient_observations);
    EXPECT_EQ(forecast_evidence_artifact(insufficient_window)->containers[0].status,
              EvidenceForecastStatus::unavailable_insufficient_observation_window);

    std::filesystem::remove(insufficient_count);
    std::filesystem::remove(insufficient_window);
}

TEST(EvidenceForecast, ClearsEstimatesWhenALaterMetricBecomesUnavailable) {
    const auto unavailable_current = write_artifact(
        {snapshot(100, 400), snapshot(200, 400), snapshot(300, 400), snapshot(300, 400, false)});
    const auto unavailable_maximum =
        write_artifact({snapshot(100, 400), snapshot(200, 400), snapshot(300, 400),
                        snapshot(300, 400, true, false)});

    const auto current_result = forecast_evidence_artifact(unavailable_current);
    ASSERT_TRUE(current_result.has_value());
    ASSERT_EQ(current_result->containers.size(), 1U);
    EXPECT_EQ(current_result->containers[0].status,
              EvidenceForecastStatus::unavailable_current_memory);
    EXPECT_FALSE(current_result->containers[0].growth_bytes_per_second.has_value());
    EXPECT_FALSE(current_result->containers[0].estimated_time_to_memory_max.has_value());

    const auto maximum_result = forecast_evidence_artifact(unavailable_maximum);
    ASSERT_TRUE(maximum_result.has_value());
    ASSERT_EQ(maximum_result->containers.size(), 1U);
    EXPECT_EQ(maximum_result->containers[0].status, EvidenceForecastStatus::unavailable_memory_max);
    EXPECT_FALSE(maximum_result->containers[0].growth_bytes_per_second.has_value());
    EXPECT_FALSE(maximum_result->containers[0].estimated_time_to_memory_max.has_value());

    std::filesystem::remove(unavailable_current);
    std::filesystem::remove(unavailable_maximum);
}

} // namespace
} // namespace memoptima::forecast
