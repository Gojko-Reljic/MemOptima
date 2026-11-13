#include <memoptima/evidence_record_parser.hpp>

#include <gtest/gtest.h>

namespace memoptima {
namespace {

TEST(EvidenceRecordParser, ParsesSupportedRootRecords) {
    const auto parsed =
        parse_evidence_record(R"({"record_type":"session_start","schema_version":1})", 7);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->type, EvidenceRecordType::session_start);
    EXPECT_EQ(parsed->canonical_json, R"({"record_type":"session_start","schema_version":1})");
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

} // namespace
} // namespace memoptima
