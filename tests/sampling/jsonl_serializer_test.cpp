#include <memoptima/sampling/jsonl_serializer.hpp>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace memoptima::sampling {
namespace {

using Json = nlohmann::json;

[[nodiscard]] CgroupMetricReadError unavailable(std::string source) {
    return CgroupMetricReadError{
        .code = CgroupMetricReadErrorCode::file_read_failed,
        .source_name = std::move(source),
        .detail = "The fixture could not read this metric.",
    };
}

[[nodiscard]] DockerContainerMemorySnapshot make_snapshot(std::string id) {
    const auto parsed_id = DockerContainerId::parse(id);
    EXPECT_TRUE(parsed_id.has_value());
    return DockerContainerMemorySnapshot::create(
        *parsed_id, "/sys/fs/cgroup/docker/" + id,
        CgroupMemorySnapshot::create(
            CgroupByteMetric{std::unexpected(unavailable("memory.current"))},
            CgroupMemoryLimits{
                .high = CgroupMemoryLimitMetric{std::optional<std::uint64_t>{}},
                .maximum = CgroupMemoryLimitMetric{std::unexpected(unavailable("memory.max"))}},
            CgroupMemoryEvents{.high = CgroupEventMetric{1},
                               .maximum = CgroupEventMetric{2},
                               .oom = CgroupEventMetric{3},
                               .oom_kill = CgroupEventMetric{4}},
            CgroupMemoryComposition{.anonymous =
                                        CgroupByteMetric{std::numeric_limits<std::uint64_t>::max()},
                                    .file_backed = CgroupByteMetric{128}},
            CgroupByteMetric{64},
            CgroupMemoryPressure{
                .some = CgroupMetric<CgroupPressureLine>{CgroupPressureLine{
                    .average_10_seconds = 1.25,
                    .average_60_seconds = 2.5,
                    .average_300_seconds = 3.75,
                    .total_microseconds = std::numeric_limits<std::uint64_t>::max(),
                }},
                .full = CgroupMetric<CgroupPressureLine>{
                    std::unexpected(unavailable("memory.pressure"))}}));
}

TEST(SamplingJsonlSerializer, SerializesSessionStartWithPrivacyAndDurabilityContract) {
    const auto count = SamplingCount::create(3);
    const auto interval = SamplingInterval::create(std::chrono::seconds{2});
    ASSERT_TRUE(count.has_value());
    ASSERT_TRUE(interval.has_value());

    const auto serialized = serialize_session_start(
        *count, *interval, std::chrono::system_clock::time_point{std::chrono::milliseconds{123}});

    ASSERT_TRUE(serialized.has_value());
    const Json record = Json::parse(*serialized);
    EXPECT_EQ(record.at("record_type"), "session_start");
    EXPECT_EQ(record.at("schema_version"), 1);
    EXPECT_EQ(record.at("started_at_utc"), "1970-01-01T00:00:00.123Z");
    EXPECT_EQ(record.at("requested_sample_count"), 3);
    EXPECT_EQ(record.at("requested_interval_ms"), "2000");
    EXPECT_EQ(record.at("output_max_bytes"), "536870912");
    EXPECT_EQ(record.at("output_permissions_octal"), "0600");
    EXPECT_TRUE(record.at("flush_after_each_record"));
    EXPECT_FALSE(record.at("host_identifiers_included"));
}

TEST(SamplingJsonlSerializer, PreservesRawEvidenceAndPartialDataInSampleRecord) {
    const auto sequence = SamplingCount::create(2);
    ASSERT_TRUE(sequence.has_value());
    const std::string id(64, 'a');
    const DockerContainerScanWarning warning{
        .code = DockerContainerScanWarningCode::invalid_container_identifier,
        .cgroup_path = "/sys/fs/cgroup/docker/not-an-id",
        .detail = "Ignoring a directory without a valid Docker container ID.",
    };
    const SamplingEvidenceSample sample = SamplingEvidenceSample::create(
        *sequence, std::chrono::seconds{5},
        std::chrono::system_clock::time_point{std::chrono::milliseconds{123456789}},
        std::chrono::milliseconds{17}, {DockerContainerEvidence::create(make_snapshot(id))}, 1,
        {warning});

    const auto serialized = serialize_sample(sample);

    ASSERT_TRUE(serialized.has_value());
    const Json record = Json::parse(*serialized);
    EXPECT_EQ(record.at("record_type"), "sample");
    EXPECT_EQ(record.at("sample_sequence"), 2);
    EXPECT_EQ(record.at("planned_offset_ms"), "5000");
    EXPECT_EQ(record.at("captured_at_utc"), "1970-01-02T10:17:36.789Z");
    EXPECT_EQ(record.at("schedule_delay_ms"), "17");
    EXPECT_EQ(record.at("skipped_container_count"), "1");
    ASSERT_EQ(record.at("discovery_warnings").size(), 1U);
    EXPECT_EQ(record.at("discovery_warnings").front().at("code"), "invalid_container_identifier");

    ASSERT_EQ(record.at("containers").size(), 1U);
    const Json &container = record.at("containers").front();
    EXPECT_EQ(container.at("container_id"), id);
    const Json &memory = container.at("memory");
    EXPECT_TRUE(memory.at("current_bytes").is_null());
    EXPECT_EQ(memory.at("high_bytes"), "max");
    EXPECT_TRUE(memory.at("max_bytes").is_null());
    EXPECT_EQ(memory.at("stat").at("anonymous_bytes"), "18446744073709551615");
    EXPECT_EQ(memory.at("psi").at("some").at("average_10_seconds"), 1.25);
    EXPECT_EQ(memory.at("psi").at("some").at("total_microseconds"), "18446744073709551615");
    EXPECT_TRUE(memory.at("psi").at("full").is_null());
    ASSERT_EQ(memory.at("unavailable_metrics").size(), 3U);
    EXPECT_EQ(memory.at("unavailable_metrics").at(0).at("source"), "memory.current");
    EXPECT_EQ(container.at("assessment").at("coverage"), "partial");
}

TEST(SamplingJsonlSerializer, SerializesCompletedAndFailedSessionEndRecords) {
    const auto ended_at = std::chrono::system_clock::time_point{std::chrono::milliseconds{1000}};

    const auto completed =
        serialize_session_end(SamplingSessionOutcome::completed, 2, ended_at, std::nullopt);
    ASSERT_TRUE(completed.has_value());
    const Json completed_record = Json::parse(*completed);
    EXPECT_EQ(completed_record.at("outcome"), "completed");
    EXPECT_EQ(completed_record.at("completed_sample_count"), 2);
    EXPECT_TRUE(completed_record.at("failure").is_null());

    const auto failed = serialize_session_end(
        SamplingSessionOutcome::failed, 1, ended_at,
        SamplingSessionFailure{.code = SamplingSessionFailureCode::output_size_limit_reached,
                               .detail = "The output artifact reached its size limit."});
    ASSERT_TRUE(failed.has_value());
    const Json failed_record = Json::parse(*failed);
    EXPECT_EQ(failed_record.at("outcome"), "failed");
    EXPECT_EQ(failed_record.at("failure").at("code"), "output_size_limit_reached");
}

TEST(SamplingJsonlSerializer, RejectsAContradictorySessionEnd) {
    const auto serialized = serialize_session_end(
        SamplingSessionOutcome::completed, 1, std::chrono::system_clock::time_point{},
        SamplingSessionFailure{.code = SamplingSessionFailureCode::output_write_failed,
                               .detail = "This must not accompany a completed outcome."});

    ASSERT_FALSE(serialized.has_value());
    EXPECT_EQ(serialized.error().code, SamplingJsonlSerializationErrorCode::invalid_session_end);
}

} // namespace
} // namespace memoptima::sampling
