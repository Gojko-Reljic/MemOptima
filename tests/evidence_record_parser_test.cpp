#include <memoptima/evidence_record_parser.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(EvidenceRecordParser, ParsesSupportedRootRecords) {
    const auto parsed = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})",
        7);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->type, EvidenceRecordType::sample);
    EXPECT_FALSE(parsed->canonical_json.empty());
}

TEST(EvidenceRecordParser, ReportsPreciseRootValidationErrors) {
    const auto invalid_json = parse_evidence_record("{", 3);
    const auto unsupported_version =
        parse_evidence_record(R"({"record_type":"sample","schema_version":2})", 4);
    const auto unknown_type =
        parse_evidence_record(R"({"record_type":"other","schema_version":1})", 5);

    ASSERT_FALSE(invalid_json.has_value());
    ASSERT_FALSE(unsupported_version.has_value());
    ASSERT_FALSE(unknown_type.has_value());
    EXPECT_EQ(invalid_json.error().front().line_number, 3U);
    EXPECT_EQ(invalid_json.error().front().code, EvidenceValidationErrorCode::invalid_json);
    EXPECT_EQ(unsupported_version.error().front().json_path, "$.schema_version");
    EXPECT_EQ(unsupported_version.error().front().code,
              EvidenceValidationErrorCode::unsupported_schema_version);
    EXPECT_EQ(unknown_type.error().front().json_path, "$.record_type");
    EXPECT_EQ(unknown_type.error().front().code, EvidenceValidationErrorCode::invalid_record_type);
}

TEST(EvidenceRecordParser, ValidatesClosedSessionStartAndSessionEndRecords) {
    const auto start = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"application_version":"0.4.0","started_at_utc":"2026-11-13T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})",
        1);
    const auto end = parse_evidence_record(
        R"({"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":1,"ended_at_utc":"2026-11-13T12:00:00.100Z","failure":null})",
        2);

    EXPECT_TRUE(start.has_value());
    EXPECT_TRUE(end.has_value());
}

TEST(EvidenceRecordParser, RejectsInvalidStartAndEndContracts) {
    const auto unknown = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"extra":true})", 9);
    const auto bad_permissions = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"application_version":"0.4.0","started_at_utc":"x","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"1","output_permissions_octal":"0644","flush_after_each_record":true,"host_identifiers_included":false})",
        10);
    const auto contradictory_end = parse_evidence_record(
        R"({"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":0,"ended_at_utc":"x","failure":{"code":"output_write_failed"}})",
        11);

    ASSERT_FALSE(unknown.has_value());
    ASSERT_FALSE(bad_permissions.has_value());
    ASSERT_FALSE(contradictory_end.has_value());
    EXPECT_EQ(unknown.error().front().code, EvidenceValidationErrorCode::missing_required_field);
    EXPECT_EQ(bad_permissions.error().back().json_path, "$.output_permissions_octal");
    EXPECT_EQ(contradictory_end.error().back().json_path, "$.failure");
}

TEST(EvidenceRecordParser, ValidatesTheSampleRecordEnvelope) {
    const auto sample = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})",
        4);
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":0,"planned_offset_ms":"00","captured_at_utc":1,"schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":{},"containers":[]})",
        5);

    EXPECT_TRUE(sample.has_value());
    ASSERT_FALSE(invalid.has_value());
    EXPECT_GE(invalid.error().size(), 3U);
}

TEST(EvidenceRecordParser, RejectsInvalidContainerAssessmentBeforeAggregation) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{},"assessment":{"overall_risk":"unknown","coverage":"complete","findings":[],"unavailable_evidence":[],"extra":true}}]})",
        12);

    ASSERT_FALSE(invalid.has_value());
    ASSERT_GE(invalid.error().size(), 2U);
    EXPECT_EQ(invalid.error().back().json_path, "$.containers[0].assessment.overall_risk");
}

TEST(EvidenceRecordParser, RejectsInvalidRawMemoryMetricTypes) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{"current_bytes":"1","high_bytes":"max","max_bytes":"2","events":{"high":"0","max":"0","oom":"0","oom_kill":1},"stat":{"anonymous_bytes":"1","file_backed_bytes":"1"},"swap_current_bytes":"0","psi":{},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]})",
        13);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().back().json_path, "$.containers[0].memory.events.oom_kill");
}

} // namespace
} // namespace memoptima
