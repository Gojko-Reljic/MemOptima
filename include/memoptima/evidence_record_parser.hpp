#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

namespace memoptima {

enum class EvidenceRecordType {
    session_start,
    sample,
    session_end,
};

enum class EvidenceValidationErrorCode {
    invalid_json,
    nesting_too_deep,
    duplicate_key,
    root_not_object,
    missing_required_field,
    wrong_type,
    unknown_field,
    unsupported_schema_version,
    invalid_record_type,
    invalid_value,
};

struct EvidenceValidationError {
    std::uint64_t line_number;
    // This path can contain untrusted JSON member names and must be escaped before terminal output.
    std::string json_path;
    EvidenceValidationErrorCode code;
    std::string detail;
};

struct ParsedEvidenceRecord {
    EvidenceRecordType type;
    std::string canonical_json;
};

// Validates one closed-schema JSONL record before inspection aggregates any of its values.
[[nodiscard]] std::expected<ParsedEvidenceRecord, std::vector<EvidenceValidationError>>
parse_evidence_record(std::string_view line, std::uint64_t line_number);

} // namespace memoptima
