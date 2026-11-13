#include <memoptima/evidence_inspector.hpp>

#include <gtest/gtest.h>

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
    EXPECT_EQ(completed->completed_sample_count, 1U);
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

} // namespace
} // namespace memoptima
