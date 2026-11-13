#include <memoptima/evidence_record_parser.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace memoptima {
namespace {

using Json = nlohmann::ordered_json;

void add_error(std::vector<EvidenceValidationError> &errors, std::uint64_t line, std::string path,
               EvidenceValidationErrorCode code, std::string detail) {
    errors.push_back({line, std::move(path), code, std::move(detail)});
}

bool exact_keys(const Json &value, std::initializer_list<std::string_view> keys, std::uint64_t line,
                std::vector<EvidenceValidationError> &errors) {
    for (const auto key : keys)
        if (!value.contains(key))
            add_error(errors, line, "$." + std::string{key},
                      EvidenceValidationErrorCode::missing_required_field,
                      "The required field is missing.");
    for (const auto &item : value.items()) {
        bool known{};
        for (const auto key : keys)
            known = known || item.key() == key;
        if (!known)
            add_error(errors, line, "$." + item.key(), EvidenceValidationErrorCode::unknown_field,
                      "The field is not defined by schema version 1.");
    }
    return errors.empty();
}

bool string_field(const Json &value, std::string_view key, std::uint64_t line,
                  std::vector<EvidenceValidationError> &errors) {
    if (!value.at(key).is_string()) {
        add_error(errors, line, "$." + std::string{key}, EvidenceValidationErrorCode::wrong_type,
                  "Expected a string.");
        return false;
    }
    return true;
}

bool decimal_string(const Json &value, std::string_view key, std::uint64_t line,
                    std::vector<EvidenceValidationError> &errors) {
    if (!string_field(value, key, line, errors))
        return false;
    const auto text = value.at(key).get<std::string>();
    const bool valid = !text.empty() && !(text.size() > 1 && text.front() == '0') &&
                       std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; });
    if (!valid)
        add_error(errors, line, "$." + std::string{key}, EvidenceValidationErrorCode::invalid_value,
                  "Expected a canonical unsigned decimal string.");
    return valid;
}

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
    std::vector<EvidenceValidationError> errors;
    if (parsed_type == EvidenceRecordType::session_start) {
        exact_keys(value,
                   {"record_type", "schema_version", "application_version", "started_at_utc",
                    "requested_sample_count", "requested_interval_ms", "output_max_bytes",
                    "output_permissions_octal", "flush_after_each_record",
                    "host_identifiers_included"},
                   line_number, errors);
        for (const auto key : {"application_version", "started_at_utc", "output_permissions_octal"})
            if (value.contains(key))
                string_field(value, key, line_number, errors);
        for (const auto key : {"requested_interval_ms", "output_max_bytes"})
            if (value.contains(key))
                decimal_string(value, key, line_number, errors);
        if (value.contains("requested_sample_count") &&
            (!value.at("requested_sample_count").is_number_unsigned() ||
             value.at("requested_sample_count") == 0))
            add_error(errors, line_number, "$.requested_sample_count",
                      EvidenceValidationErrorCode::invalid_value,
                      "Expected a positive unsigned JSON number.");
        if (value.contains("output_permissions_octal") &&
            value.at("output_permissions_octal") != "0600")
            add_error(errors, line_number, "$.output_permissions_octal",
                      EvidenceValidationErrorCode::invalid_value,
                      "Expected the owner-only permission value 0600.");
        for (const auto key : {"flush_after_each_record", "host_identifiers_included"})
            if (value.contains(key) && !value.at(key).is_boolean())
                add_error(errors, line_number, "$." + std::string{key},
                          EvidenceValidationErrorCode::wrong_type, "Expected a boolean.");
    }
    if (parsed_type == EvidenceRecordType::session_end) {
        exact_keys(value,
                   {"record_type", "schema_version", "outcome", "completed_sample_count",
                    "ended_at_utc", "failure"},
                   line_number, errors);
        for (const auto key : {"outcome", "ended_at_utc"})
            if (value.contains(key))
                string_field(value, key, line_number, errors);
        if (value.contains("completed_sample_count") &&
            !value.at("completed_sample_count").is_number_unsigned())
            add_error(errors, line_number, "$.completed_sample_count",
                      EvidenceValidationErrorCode::wrong_type, "Expected an unsigned JSON number.");
        if (value.contains("outcome") && value.at("outcome").is_string()) {
            const auto outcome = value.at("outcome").get<std::string>();
            const bool failed = outcome == "failed";
            if (outcome != "completed" && outcome != "interrupted" && !failed)
                add_error(errors, line_number, "$.outcome",
                          EvidenceValidationErrorCode::invalid_value,
                          "Expected completed, interrupted, or failed.");
            if (value.contains("failure") && ((failed && !value.at("failure").is_object()) ||
                                              (!failed && !value.at("failure").is_null())))
                add_error(errors, line_number, "$.failure",
                          EvidenceValidationErrorCode::invalid_value,
                          "Failure must match the terminal outcome.");
        }
    }
    if (parsed_type == EvidenceRecordType::sample) {
        exact_keys(value,
                   {"record_type", "schema_version", "sample_sequence", "planned_offset_ms",
                    "captured_at_utc", "schedule_delay_ms", "skipped_container_count",
                    "discovery_warnings", "containers"},
                   line_number, errors);
        if (value.contains("sample_sequence") &&
            (!value.at("sample_sequence").is_number_unsigned() || value.at("sample_sequence") == 0))
            add_error(errors, line_number, "$.sample_sequence",
                      EvidenceValidationErrorCode::invalid_value,
                      "Expected a positive unsigned JSON number.");
        for (const auto key : {"planned_offset_ms", "schedule_delay_ms", "skipped_container_count"})
            if (value.contains(key))
                decimal_string(value, key, line_number, errors);
        if (value.contains("captured_at_utc"))
            string_field(value, "captured_at_utc", line_number, errors);
        for (const auto key : {"discovery_warnings", "containers"})
            if (value.contains(key) && !value.at(key).is_array())
                add_error(errors, line_number, "$." + std::string{key},
                          EvidenceValidationErrorCode::wrong_type, "Expected an array.");
        if (value.contains("containers") && value.at("containers").is_array())
            for (std::size_t index{}; index < value.at("containers").size(); ++index) {
                const auto &container = value.at("containers").at(index);
                const auto path = "$.containers[" + std::to_string(index) + "]";
                if (!container.is_object()) {
                    add_error(errors, line_number, path, EvidenceValidationErrorCode::wrong_type,
                              "Expected an object.");
                    continue;
                }
                exact_keys(container, {"container_id", "cgroup_path", "memory", "assessment"},
                           line_number, errors);
                for (const auto key : {"container_id", "cgroup_path"})
                    if (container.contains(key) && !container.at(key).is_string())
                        add_error(errors, line_number, path + "." + std::string{key},
                                  EvidenceValidationErrorCode::wrong_type, "Expected a string.");
                if (container.contains("memory") && !container.at("memory").is_object())
                    add_error(errors, line_number, path + ".memory",
                              EvidenceValidationErrorCode::wrong_type, "Expected an object.");
                if (container.contains("memory") && container.at("memory").is_object())
                    exact_keys(container.at("memory"),
                               {"current_bytes", "high_bytes", "max_bytes", "events", "stat",
                                "swap_current_bytes", "psi", "unavailable_metrics"},
                               line_number, errors);
                if (container.contains("assessment") && !container.at("assessment").is_object())
                    add_error(errors, line_number, path + ".assessment",
                              EvidenceValidationErrorCode::wrong_type, "Expected an object.");
                if (container.contains("assessment") && container.at("assessment").is_object()) {
                    const auto &assessment = container.at("assessment");
                    exact_keys(assessment,
                               {"overall_risk", "coverage", "findings", "unavailable_evidence"},
                               line_number, errors);
                    if (assessment.contains("overall_risk") &&
                        (!assessment.at("overall_risk").is_string() ||
                         (assessment.at("overall_risk") != "low" &&
                          assessment.at("overall_risk") != "moderate" &&
                          assessment.at("overall_risk") != "high" &&
                          assessment.at("overall_risk") != "critical")))
                        add_error(errors, line_number, path + ".assessment.overall_risk",
                                  EvidenceValidationErrorCode::invalid_value,
                                  "Expected a known risk level.");
                    if (assessment.contains("coverage") &&
                        (!assessment.at("coverage").is_string() ||
                         (assessment.at("coverage") != "complete" &&
                          assessment.at("coverage") != "partial")))
                        add_error(errors, line_number, path + ".assessment.coverage",
                                  EvidenceValidationErrorCode::invalid_value,
                                  "Expected complete or partial coverage.");
                }
            }
    }
    if (!errors.empty())
        if (errors.size() > 20)
            errors.resize(20);
    if (!errors.empty())
        return std::unexpected(std::move(errors));
    return ParsedEvidenceRecord{.type = parsed_type, .canonical_json = value.dump()};
}

} // namespace memoptima
