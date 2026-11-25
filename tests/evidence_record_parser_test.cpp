#include <memoptima/evidence_record_parser.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <ranges>

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

TEST(EvidenceRecordParser, RejectsInvalidTimestampsOverflowAndFailureObjects) {
    const auto invalid_start = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"application_version":"0.5.0","started_at_utc":"2026-11-13","requested_sample_count":1,"requested_interval_ms":"18446744073709551616","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})",
        16);
    const auto invalid_end = parse_evidence_record(
        R"({"record_type":"session_end","schema_version":1,"outcome":"failed","completed_sample_count":0,"ended_at_utc":"2026-11-13T12:00:00.000Z","failure":{"code":"unknown","detail":1,"extra":true}})",
        17);

    ASSERT_FALSE(invalid_start.has_value());
    ASSERT_FALSE(invalid_end.has_value());
    EXPECT_TRUE(
        std::ranges::any_of(invalid_start.error(), [](const EvidenceValidationError &error) {
            return error.json_path == "$.started_at_utc";
        }));
    EXPECT_TRUE(
        std::ranges::any_of(invalid_start.error(), [](const EvidenceValidationError &error) {
            return error.json_path == "$.requested_interval_ms";
        }));
    EXPECT_TRUE(std::ranges::any_of(invalid_end.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.failure.extra";
    }));
    EXPECT_TRUE(std::ranges::any_of(invalid_end.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.failure.code";
    }));

    const auto invalid_calendar = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"application_version":"0.5.0","started_at_utc":"2026-02-30T12:00:00.000Z","requested_sample_count":1,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})",
        18);
    ASSERT_FALSE(invalid_calendar.has_value());
    EXPECT_EQ(invalid_calendar.error().front().json_path, "$.started_at_utc");
}

TEST(EvidenceRecordParser, EnforcesTrustBoundaryScalarConstraints) {
    const auto invalid_application_version = parse_evidence_record(
        R"({"record_type":"session_start","schema_version":1,"application_version":"0.9.0\u001b","started_at_utc":"2026-11-13T12:00:00.000Z","requested_sample_count":4294967296,"requested_interval_ms":"100","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false})",
        19);
    const auto invalid_sample_sequence = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":4294967296,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[]})",
        20);
    const auto invalid_container_id = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"gggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggggg","cgroup_path":"/docker/example","memory":{},"assessment":{}}]})",
        21);

    ASSERT_FALSE(invalid_application_version.has_value());
    ASSERT_FALSE(invalid_sample_sequence.has_value());
    ASSERT_FALSE(invalid_container_id.has_value());
    EXPECT_TRUE(std::ranges::any_of(invalid_application_version.error(),
                                    [](const EvidenceValidationError &error) {
                                        return error.json_path == "$.application_version";
                                    }));
    EXPECT_TRUE(std::ranges::any_of(invalid_application_version.error(),
                                    [](const EvidenceValidationError &error) {
                                        return error.json_path == "$.requested_sample_count";
                                    }));
    EXPECT_EQ(invalid_sample_sequence.error().front().json_path, "$.sample_sequence");
    EXPECT_TRUE(
        std::ranges::any_of(invalid_container_id.error(), [](const EvidenceValidationError &error) {
            return error.json_path == "$.containers[0].container_id";
        }));
}

TEST(EvidenceRecordParser, RejectsMalformedWarningsAndFindingEvidence) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[{"code":"warning","cgroup_path":"/x","detail":1,"extra":true}],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{},"assessment":{"overall_risk":"low","coverage":"complete","findings":[{"code":"finding","risk":"high","evidence":[{"code":"metric","value":"18446744073709551616","extra":true}],"recommendation":"act"}],"unavailable_evidence":[]}}]})",
        18);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.discovery_warnings[0].detail";
    }));
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.containers[0].assessment.findings[0].evidence[0].extra";
    }));
}

TEST(EvidenceRecordParser, RejectsWrongNestedCollectionTypes) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{"unavailable_metrics":{}},"assessment":{"overall_risk":"low","coverage":"complete","findings":{},"unavailable_evidence":[]}}]})",
        19);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.containers[0].memory.unavailable_metrics";
    }));
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.containers[0].assessment.findings";
    }));
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
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.containers[0].memory.events.oom_kill";
    }));
}

TEST(EvidenceRecordParser, RejectsPsiAveragesOutsideTheSupportedRange) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{"current_bytes":"1","high_bytes":"max","max_bytes":"2","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"1","file_backed_bytes":"1"},"swap_current_bytes":"0","psi":{"some":{"average_10_seconds":101.0,"average_60_seconds":0.0,"average_300_seconds":0.0,"total_microseconds":"0"},"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]})",
        14);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error().back().json_path,
              "$.containers[0].memory.psi.some.average_10_seconds");
}

TEST(EvidenceRecordParser, RejectsMalformedUnavailableMetricDiagnostics) {
    const auto invalid = parse_evidence_record(
        R"({"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-11-13T12:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"a","cgroup_path":"/cgroup/a","memory":{"current_bytes":null,"high_bytes":"max","max_bytes":"2","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"1","file_backed_bytes":"1"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[{"code":"file_read_failed","source":"memory.current","detail":1}]},"assessment":{"overall_risk":"low","coverage":"partial","findings":[],"unavailable_evidence":[]}}]})",
        15);

    ASSERT_FALSE(invalid.has_value());
    EXPECT_TRUE(std::ranges::any_of(invalid.error(), [](const EvidenceValidationError &error) {
        return error.json_path == "$.containers[0].memory.unavailable_metrics[0].detail";
    }));
}

} // namespace
} // namespace memoptima
