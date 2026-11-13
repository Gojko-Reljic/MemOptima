#include <memoptima/evidence_record_parser.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace memoptima {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] std::unexpected<std::vector<EvidenceValidationError>>
error(std::uint64_t line_number, std::string path, EvidenceValidationErrorCode code,
      std::string detail) {
    return std::unexpected(std::vector<EvidenceValidationError>{EvidenceValidationError{
        .line_number = line_number,
        .json_path = std::move(path),
        .code = code,
        .detail = std::move(detail),
    }});
}

} // namespace

std::expected<ParsedEvidenceRecord, std::vector<EvidenceValidationError>>
parse_evidence_record(std::string_view line, std::uint64_t line_number) {
    const Json value = Json::parse(line, nullptr, false);
    if (value.is_discarded()) {
        return error(line_number, "$", EvidenceValidationErrorCode::invalid_json,
                     "The line is not valid JSON.");
    }
    if (!value.is_object()) {
        return error(line_number, "$", EvidenceValidationErrorCode::root_not_object,
                     "The JSON root must be an object.");
    }
    if (!value.contains("schema_version")) {
        return error(line_number, "$.schema_version",
                     EvidenceValidationErrorCode::missing_required_field,
                     "The required field is missing.");
    }
    if (!value.at("schema_version").is_number_unsigned()) {
        return error(line_number, "$.schema_version", EvidenceValidationErrorCode::wrong_type,
                     "Expected an unsigned JSON number.");
    }
    if (value.at("schema_version") != 1) {
        return error(line_number, "$.schema_version",
                     EvidenceValidationErrorCode::unsupported_schema_version,
                     "Only schema_version 1 is supported.");
    }
    if (!value.contains("record_type")) {
        return error(line_number, "$.record_type",
                     EvidenceValidationErrorCode::missing_required_field,
                     "The required field is missing.");
    }
    if (!value.at("record_type").is_string()) {
        return error(line_number, "$.record_type", EvidenceValidationErrorCode::wrong_type,
                     "Expected a string.");
    }
    const std::string type = value.at("record_type").get<std::string>();
    EvidenceRecordType parsed_type;
    if (type == "session_start")
        parsed_type = EvidenceRecordType::session_start;
    else if (type == "sample")
        parsed_type = EvidenceRecordType::sample;
    else if (type == "session_end")
        parsed_type = EvidenceRecordType::session_end;
    else
        return error(line_number, "$.record_type", EvidenceValidationErrorCode::invalid_record_type,
                     "Expected session_start, sample, or session_end.");
    return ParsedEvidenceRecord{.type = parsed_type, .canonical_json = value.dump()};
}

} // namespace memoptima
