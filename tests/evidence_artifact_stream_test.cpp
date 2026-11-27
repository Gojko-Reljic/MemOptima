#include <memoptima/evidence/evidence_artifact_stream.hpp>
#include <memoptima/sampling/evidence_models.hpp>
#include <memoptima/sampling/jsonl_serializer.hpp>
#include <memoptima/sampling/sampling_configuration.hpp>

#include "evidence_artifact_reader.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

namespace memoptima::evidence {
namespace {

class EvidenceArtifactStreamTest : public testing::Test {
  protected:
    void SetUp() override {
        directory_ = std::filesystem::temp_directory_path() /
                     ("memoptima-evidence-stream-" + std::to_string(::getpid()) + "-" +
                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(directory_));
    }

    void TearDown() override {
        std::error_code error;
        std::filesystem::remove_all(directory_, error);
        EXPECT_FALSE(error) << error.message();
    }

    std::filesystem::path write_artifact() const {
        const auto count = sampling::SamplingCount::create(1U);
        const auto interval = sampling::SamplingInterval::create(std::chrono::seconds{10});
        EXPECT_TRUE(count.has_value());
        EXPECT_TRUE(interval.has_value());

        const auto path = directory_ / "sample.jsonl";
        std::ofstream output{path};
        EXPECT_TRUE(output.is_open());
        output << *sampling::serialize_session_start(*count, *interval,
                                                     std::chrono::system_clock::time_point{})
               << '\n';
        output
            << R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"1970-01-01T00:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/example","memory":{"current_bytes":"100","high_bytes":"max","max_bytes":"400","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]})"
            << '\n';
        output << *sampling::serialize_session_end(sampling::SamplingSessionOutcome::completed, 1U,
                                                   std::chrono::system_clock::time_point{} +
                                                       std::chrono::seconds{10},
                                                   std::nullopt)
               << '\n';
        return path;
    }

    std::filesystem::path write_malformed_artifact() const {
        const auto count = sampling::SamplingCount::create(1U);
        const auto interval = sampling::SamplingInterval::create(std::chrono::seconds{10});
        EXPECT_TRUE(count.has_value());
        EXPECT_TRUE(interval.has_value());

        const auto path = directory_ / "malformed.jsonl";
        std::ofstream output{path};
        EXPECT_TRUE(output.is_open());
        output << *sampling::serialize_session_start(*count, *interval,
                                                     std::chrono::system_clock::time_point{})
               << '\n';
        output << "not JSON\n";
        return path;
    }

    std::filesystem::path directory_;
};

class CapturingSink final : public ArtifactSink {
  public:
    std::expected<void, ArtifactTraversalError>
    consume(const ArtifactSessionStart &start) override {
        starts.push_back(start);
        return {};
    }

    std::expected<void, ArtifactTraversalError> consume(const ArtifactSample &sample) override {
        samples.push_back(sample);
        return {};
    }

    std::expected<void, ArtifactTraversalError> consume(const ArtifactSessionEnd &end) override {
        ends.push_back(end);
        return {};
    }

    std::vector<ArtifactSessionStart> starts;
    std::vector<ArtifactSample> samples;
    std::vector<ArtifactSessionEnd> ends;
};

class DiscardingRecordSink final : public detail::EvidenceArtifactRecordSink {
  public:
    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::EvidenceArtifactRecord &) override {
        return {};
    }
};

TEST_F(EvidenceArtifactStreamTest, StreamsValidatedEvidenceThroughThePublicContract) {
    (void)write_artifact();
    CapturingSink sink;

    const auto traversal = traverse_artifact_beneath(directory_, "sample.jsonl", sink);

    ASSERT_TRUE(traversal.has_value()) << traversal.error().detail;
    EXPECT_FALSE(traversal->incomplete);
    ASSERT_EQ(sink.starts.size(), 1U);
    EXPECT_EQ(sink.starts[0].started_at_utc, "1970-01-01T00:00:00.000Z");
    EXPECT_EQ(sink.starts[0].requested_interval_ms, 10'000U);
    ASSERT_EQ(sink.samples.size(), 1U);
    EXPECT_EQ(sink.samples[0].captured_at_utc, "1970-01-01T00:00:00.000Z");
    ASSERT_EQ(sink.samples[0].containers.size(), 1U);
    const auto &container = sink.samples[0].containers[0];
    EXPECT_EQ(container.container_id,
              "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    EXPECT_EQ(container.cgroup_path, "/docker/example");
    EXPECT_EQ(container.current_memory_bytes, 100U);
    EXPECT_EQ(container.maximum_memory.kind, ArtifactMemoryMaximumKind::finite);
    EXPECT_EQ(container.maximum_memory.bytes, 400U);
    ASSERT_EQ(sink.ends.size(), 1U);
    EXPECT_EQ(sink.ends[0].outcome, ArtifactSessionOutcome::completed);
}

TEST_F(EvidenceArtifactStreamTest, DeliversProvisionalEventsBeforeALaterValidationFailure) {
    (void)write_malformed_artifact();
    CapturingSink sink;

    const auto traversal = traverse_artifact_beneath(directory_, "malformed.jsonl", sink);

    ASSERT_FALSE(traversal.has_value());
    EXPECT_FALSE(traversal.error().detail.empty());
    ASSERT_EQ(sink.starts.size(), 1U);
    EXPECT_TRUE(sink.samples.empty());
    EXPECT_TRUE(sink.ends.empty());
}

TEST_F(EvidenceArtifactStreamTest, PreservesValidationErrorsBeforeRecordOrderErrors) {
    const auto path = directory_ / "duplicate-start.jsonl";
    std::ofstream output{path};
    ASSERT_TRUE(output.is_open());
    output
        << R"({"record_type":"session_start","schema_version":1,"application_version":"0.10.0","started_at_utc":"2026-11-26T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
        << '\n';
    output << '\n';
    output << R"({"record_type":"sample","record_type":"sample"})" << '\n';
    output
        << R"({"record_type":"session_start","schema_version":1,"application_version":"0.10.0","started_at_utc":"2026-11-26T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
        << '\n';
    output.close();

    DiscardingRecordSink sink;
    const auto traversal = detail::traverse_evidence_artifact(path, sink);

    ASSERT_FALSE(traversal.has_value());
    EXPECT_NE(traversal.error().detail.find("Line 3 at $.record_type [duplicate_key]"),
              std::string::npos);
    EXPECT_EQ(traversal.error().detail.find("invalid record order"), std::string::npos);
}

} // namespace
} // namespace memoptima::evidence
