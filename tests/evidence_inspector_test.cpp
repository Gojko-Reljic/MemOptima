#include <memoptima/evidence_inspector.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <unistd.h>

namespace memoptima {
namespace {

TEST(EvidenceInspector, SummarizesCompletedAndIncompleteArtifacts) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("memoptima-inspect-" + std::to_string(::getpid()) + ".jsonl");
    {
        std::ofstream output{path};
        output
            << R"({"record_type":"session_start","schema_version":1,"application_version":"0.4.0","started_at_utc":"2026-11-13T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
            << '\n';
        output
            << R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})"
            << '\n';
        output
            << R"({"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":1,"ended_at_utc":"2026-11-13T12:00:00.100Z","failure":null})"
            << '\n';
    }
    const auto completed = inspect_evidence_artifact(path);
    ASSERT_TRUE(completed.has_value());
    EXPECT_FALSE(completed->incomplete);
    EXPECT_EQ(completed->schema_version, 1U);
    EXPECT_EQ(completed->requested_interval_ms, "100");
    EXPECT_EQ(completed->completed_sample_count, 1U);
    EXPECT_EQ(completed->first_capture_at_utc, "2026-11-13T12:00:00.000Z");
    EXPECT_EQ(completed->last_capture_at_utc, "2026-11-13T12:00:00.000Z");
    std::filesystem::remove(path);
}

TEST(EvidenceInspector, PreservesValidSamplesWhenTheTerminalRecordIsMissing) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("memoptima-incomplete-" + std::to_string(::getpid()) + ".jsonl");
    {
        std::ofstream output{path};
        output
            << R"({"record_type":"session_start","schema_version":1,"application_version":"0.5.0","started_at_utc":"2026-11-13T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
            << '\n';
        output
            << R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})"
            << '\n';
    }
    const auto incomplete = inspect_evidence_artifact(path);
    ASSERT_TRUE(incomplete.has_value());
    EXPECT_TRUE(incomplete->incomplete);
    EXPECT_EQ(incomplete->completed_sample_count, 1U);
    std::filesystem::remove(path);
}

TEST(EvidenceInspector, BoundsValidationDiagnosticsAcrossMultipleLines) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("memoptima-invalid-" + std::to_string(::getpid()) + ".jsonl");
    {
        std::ofstream output{path};
        for (int line{}; line < 25; ++line)
            output << "{\n";
    }

    const auto inspected = inspect_evidence_artifact(path);

    ASSERT_FALSE(inspected.has_value());
    EXPECT_EQ(std::count(inspected.error().detail.begin(), inspected.error().detail.end(), '\n'),
              19);
    EXPECT_TRUE(inspected.error().detail.starts_with("Line 1 at $"));
    EXPECT_NE(inspected.error().detail.find("Line 20 at $"), std::string::npos);
    EXPECT_EQ(inspected.error().detail.find("Line 21 at $"), std::string::npos);
    std::filesystem::remove(path);
}

TEST(EvidenceInspector, RejectsCompletedSessionMissingRequestedSamples) {
    const auto path = std::filesystem::temp_directory_path() /
                      ("memoptima-incomplete-completed-" + std::to_string(::getpid()) + ".jsonl");
    {
        std::ofstream output{path};
        output
            << R"({"record_type":"session_start","schema_version":1,"application_version":"0.5.0","started_at_utc":"2026-11-13T12:00:00.000Z","requested_sample_count":2,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})"
            << '\n';
        output
            << R"({"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":0,"ended_at_utc":"2026-11-13T12:00:00.100Z","failure":null})"
            << '\n';
    }

    const auto inspected = inspect_evidence_artifact(path);

    ASSERT_FALSE(inspected.has_value());
    EXPECT_NE(inspected.error().detail.find("must contain every requested sample"),
              std::string::npos);
    std::filesystem::remove(path);
}

} // namespace
} // namespace memoptima
