#include <memoptima/evidence_record_parser.hpp>

#include "docker_container_id_syntax.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
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

bool exact_keys(const Json &value, std::initializer_list<std::string_view> keys,
                std::string_view path, std::uint64_t line,
                std::vector<EvidenceValidationError> &errors) {
    // Evidence schemas are closed so a newer or misspelled field cannot be silently ignored.
    for (const auto key : keys)
        if (!value.contains(key))
            add_error(errors, line, std::string{path} + "." + std::string{key},
                      EvidenceValidationErrorCode::missing_required_field,
                      "The required field is missing.");
    for (const auto &item : value.items()) {
        bool known{};
        for (const auto key : keys)
            known = known || item.key() == key;
        if (!known)
            add_error(errors, line, std::string{path} + "." + item.key(),
                      EvidenceValidationErrorCode::unknown_field,
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

bool printable_application_version(const Json &value, std::uint64_t line,
                                   std::vector<EvidenceValidationError> &errors) {
    if (!string_field(value, "application_version", line, errors))
        return false;

    const auto text = value.at("application_version").get<std::string>();
    const bool valid = !text.empty() && text.size() <= 64U &&
                       std::ranges::all_of(text, [](unsigned char character) {
                           return character >= 0x20U && character <= 0x7EU;
                       });
    if (!valid)
        add_error(errors, line, "$.application_version", EvidenceValidationErrorCode::invalid_value,
                  "Expected 1 to 64 printable ASCII characters.");
    return valid;
}

bool positive_uint32_field(const Json &value, std::string_view key, std::uint64_t line,
                           std::vector<EvidenceValidationError> &errors) {
    const auto &number = value.at(key);
    if (number.is_number_unsigned() && number > 0U &&
        number <= std::numeric_limits<std::uint32_t>::max())
        return true;

    add_error(errors, line, "$." + std::string{key}, EvidenceValidationErrorCode::invalid_value,
              "Expected a positive unsigned 32-bit JSON number.");
    return false;
}

bool decimal_string(const Json &value, std::string_view key, std::uint64_t line,
                    std::vector<EvidenceValidationError> &errors) {
    if (!string_field(value, key, line, errors))
        return false;
    const auto text = value.at(key).get<std::string>();
    std::uint64_t parsed{};
    const auto [position, conversion_error] =
        std::from_chars(text.data(), text.data() + text.size(), parsed);
    const bool valid = !text.empty() && !(text.size() > 1 && text.front() == '0') &&
                       std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }) &&
                       conversion_error == std::errc{} && position == text.data() + text.size();
    if (!valid)
        add_error(errors, line, "$." + std::string{key}, EvidenceValidationErrorCode::invalid_value,
                  "Expected a canonical unsigned decimal string.");
    return valid;
}

void metric_or_null(const Json &value, std::string path, std::uint64_t line,
                    std::vector<EvidenceValidationError> &errors) {
    // null is the serialized form of unavailable collection evidence, not a numeric zero.
    if (value.is_null())
        return;
    if (!value.is_string()) {
        add_error(errors, line, std::move(path), EvidenceValidationErrorCode::wrong_type,
                  "Expected a decimal string or null.");
        return;
    }
    const auto text = value.get<std::string>();
    std::uint64_t parsed{};
    const auto [position, conversion_error] =
        std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (text.empty() || (text.size() > 1 && text.front() == '0') ||
        !std::ranges::all_of(text, [](char c) { return c >= '0' && c <= '9'; }) ||
        conversion_error != std::errc{} || position != text.data() + text.size())
        add_error(errors, line, std::move(path), EvidenceValidationErrorCode::invalid_value,
                  "Expected a canonical unsigned decimal string.");
}

void diagnostic(const Json &value, std::string path, std::uint64_t line,
                std::vector<EvidenceValidationError> &errors) {
    if (!value.is_object()) {
        add_error(errors, line, std::move(path), EvidenceValidationErrorCode::wrong_type,
                  "Expected a diagnostic object.");
        return;
    }
    exact_keys(value, {"code", "source", "detail"}, path, line, errors);
    for (const auto key : {"code", "source", "detail"})
        if (value.contains(key) && !value.at(key).is_string())
            add_error(errors, line, path + "." + key, EvidenceValidationErrorCode::wrong_type,
                      "Expected a string.");
}

void timestamp(const Json &value, std::string_view key, std::uint64_t line,
               std::vector<EvidenceValidationError> &errors) {
    if (!string_field(value, key, line, errors))
        return;
    const auto text = value.at(key).get<std::string>();
    const bool punctuation = text.size() == 24 && text[4] == '-' && text[7] == '-' &&
                             text[10] == 'T' && text[13] == ':' && text[16] == ':' &&
                             text[19] == '.' && text[23] == 'Z';
    constexpr std::array<unsigned, 17> digit_indexes{0U,  1U,  2U,  3U,  5U,  6U,  8U,  9U, 11U,
                                                     12U, 14U, 15U, 17U, 18U, 20U, 21U, 22U};
    const bool digits = punctuation && std::ranges::all_of(digit_indexes, [&text](unsigned index) {
                            return text[index] >= '0' && text[index] <= '9';
                        });
    bool valid_calendar{};
    if (digits) {
        const auto component = [&text](std::size_t offset, std::size_t length) {
            unsigned parsed{};
            const auto [position, conversion_error] =
                std::from_chars(text.data() + offset, text.data() + offset + length, parsed);
            return conversion_error == std::errc{} && position == text.data() + offset + length
                       ? parsed
                       : 0U;
        };
        const unsigned year = component(0, 4);
        const unsigned month = component(5, 2);
        const unsigned day = component(8, 2);
        const unsigned hour = component(11, 2);
        const unsigned minute = component(14, 2);
        const unsigned second = component(17, 2);
        constexpr std::array<unsigned, 12> days_per_month{31U, 28U, 31U, 30U, 31U, 30U,
                                                          31U, 31U, 30U, 31U, 30U, 31U};
        const bool leap_year = year % 4U == 0U && (year % 100U != 0U || year % 400U == 0U);
        const unsigned maximum_day = month >= 1U && month <= 12U
                                         ? days_per_month[month - 1U] + (month == 2U && leap_year)
                                         : 0U;
        valid_calendar =
            day >= 1U && day <= maximum_day && hour <= 23U && minute <= 59U && second <= 59U;
    }
    if (!valid_calendar)
        add_error(errors, line, "$." + std::string{key}, EvidenceValidationErrorCode::invalid_value,
                  "Expected an ISO 8601 UTC timestamp with milliseconds.");
}

void warning(const Json &value, std::string path, std::uint64_t line,
             std::vector<EvidenceValidationError> &errors) {
    if (!value.is_object()) {
        add_error(errors, line, std::move(path), EvidenceValidationErrorCode::wrong_type,
                  "Expected a discovery warning object.");
        return;
    }
    exact_keys(value, {"code", "cgroup_path", "detail"}, path, line, errors);
    for (const auto key : {"code", "cgroup_path", "detail"})
        if (value.contains(key) && !value.at(key).is_string())
            add_error(errors, line, path + "." + key, EvidenceValidationErrorCode::wrong_type,
                      "Expected a string.");
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
                   "$", line_number, errors);
        if (value.contains("application_version"))
            printable_application_version(value, line_number, errors);
        if (value.contains("output_permissions_octal"))
            string_field(value, "output_permissions_octal", line_number, errors);
        if (value.contains("started_at_utc"))
            timestamp(value, "started_at_utc", line_number, errors);
        for (const auto key : {"requested_interval_ms", "output_max_bytes"})
            if (value.contains(key))
                decimal_string(value, key, line_number, errors);
        if (value.contains("requested_sample_count"))
            positive_uint32_field(value, "requested_sample_count", line_number, errors);
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
                   "$", line_number, errors);
        if (value.contains("outcome"))
            string_field(value, "outcome", line_number, errors);
        if (value.contains("ended_at_utc"))
            timestamp(value, "ended_at_utc", line_number, errors);
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
            if (failed && value.contains("failure") && value.at("failure").is_object()) {
                const auto &failure = value.at("failure");
                exact_keys(failure, {"code", "detail"}, "$.failure", line_number, errors);
                for (const auto key : {"code", "detail"})
                    if (failure.contains(key) && !failure.at(key).is_string())
                        add_error(errors, line_number, "$.failure." + std::string{key},
                                  EvidenceValidationErrorCode::wrong_type, "Expected a string.");
                if (failure.contains("code") && failure.at("code").is_string() &&
                    failure.at("code") != "output_size_limit_reached" &&
                    failure.at("code") != "output_write_failed" &&
                    failure.at("code") != "output_flush_failed")
                    add_error(errors, line_number, "$.failure.code",
                              EvidenceValidationErrorCode::invalid_value,
                              "Expected a known failure code.");
            }
        }
    }
    if (parsed_type == EvidenceRecordType::sample) {
        exact_keys(value,
                   {"record_type", "schema_version", "sample_sequence", "planned_offset_ms",
                    "captured_at_utc", "schedule_delay_ms", "skipped_container_count",
                    "discovery_warnings", "containers"},
                   "$", line_number, errors);
        if (value.contains("sample_sequence"))
            positive_uint32_field(value, "sample_sequence", line_number, errors);
        for (const auto key : {"planned_offset_ms", "schedule_delay_ms", "skipped_container_count"})
            if (value.contains(key))
                decimal_string(value, key, line_number, errors);
        if (value.contains("captured_at_utc"))
            timestamp(value, "captured_at_utc", line_number, errors);
        for (const auto key : {"discovery_warnings", "containers"})
            if (value.contains(key) && !value.at(key).is_array())
                add_error(errors, line_number, "$." + std::string{key},
                          EvidenceValidationErrorCode::wrong_type, "Expected an array.");
        if (value.contains("discovery_warnings") && value.at("discovery_warnings").is_array())
            for (std::size_t index{}; index < value.at("discovery_warnings").size(); ++index)
                warning(value.at("discovery_warnings").at(index),
                        "$.discovery_warnings[" + std::to_string(index) + "]", line_number, errors);
        if (value.contains("containers") && value.at("containers").is_array())
            for (std::size_t index{}; index < value.at("containers").size(); ++index) {
                const auto &container = value.at("containers").at(index);
                const auto path = "$.containers[" + std::to_string(index) + "]";
                if (!container.is_object()) {
                    add_error(errors, line_number, path, EvidenceValidationErrorCode::wrong_type,
                              "Expected an object.");
                    continue;
                }
                exact_keys(container, {"container_id", "cgroup_path", "memory", "assessment"}, path,
                           line_number, errors);
                for (const auto key : {"container_id", "cgroup_path"})
                    if (container.contains(key) && !container.at(key).is_string())
                        add_error(errors, line_number, path + "." + std::string{key},
                                  EvidenceValidationErrorCode::wrong_type, "Expected a string.");
                if (container.contains("container_id") &&
                    container.at("container_id").is_string() &&
                    !detail::is_docker_container_id_syntax(
                        container.at("container_id").get<std::string>()))
                    add_error(errors, line_number, path + ".container_id",
                              EvidenceValidationErrorCode::invalid_value,
                              "Expected exactly 64 hexadecimal characters.");
                if (container.contains("memory") && !container.at("memory").is_object())
                    add_error(errors, line_number, path + ".memory",
                              EvidenceValidationErrorCode::wrong_type, "Expected an object.");
                if (container.contains("memory") && container.at("memory").is_object())
                    exact_keys(container.at("memory"),
                               {"current_bytes", "high_bytes", "max_bytes", "events", "stat",
                                "swap_current_bytes", "psi", "unavailable_metrics"},
                               path + ".memory", line_number, errors);
                if (container.contains("memory") && container.at("memory").is_object()) {
                    const auto &memory = container.at("memory");
                    for (const auto key : {"current_bytes", "swap_current_bytes"})
                        if (memory.contains(key))
                            metric_or_null(memory.at(key), path + ".memory." + key, line_number,
                                           errors);
                    for (const auto key : {"high_bytes", "max_bytes"})
                        if (memory.contains(key) && !memory.at(key).is_null() &&
                            memory.at(key) != "max")
                            metric_or_null(memory.at(key), path + ".memory." + key, line_number,
                                           errors);
                    for (const auto group : {"events", "stat", "psi"})
                        if (memory.contains(group) && !memory.at(group).is_object())
                            add_error(errors, line_number, path + ".memory." + group,
                                      EvidenceValidationErrorCode::wrong_type,
                                      "Expected an object.");
                    if (memory.contains("events") && memory.at("events").is_object()) {
                        exact_keys(memory.at("events"), {"high", "max", "oom", "oom_kill"},
                                   path + ".memory.events", line_number, errors);
                        for (const auto key : {"high", "max", "oom", "oom_kill"})
                            if (memory.at("events").contains(key))
                                metric_or_null(memory.at("events").at(key),
                                               path + ".memory.events." + key, line_number, errors);
                    }
                    if (memory.contains("stat") && memory.at("stat").is_object()) {
                        exact_keys(memory.at("stat"), {"anonymous_bytes", "file_backed_bytes"},
                                   path + ".memory.stat", line_number, errors);
                        for (const auto key : {"anonymous_bytes", "file_backed_bytes"})
                            if (memory.at("stat").contains(key))
                                metric_or_null(memory.at("stat").at(key),
                                               path + ".memory.stat." + key, line_number, errors);
                    }
                    if (memory.contains("psi") && memory.at("psi").is_object()) {
                        exact_keys(memory.at("psi"), {"some", "full"}, path + ".memory.psi",
                                   line_number, errors);
                        for (const auto kind : {"some", "full"}) {
                            if (!memory.at("psi").contains(kind) ||
                                memory.at("psi").at(kind).is_null())
                                continue;
                            const auto &pressure = memory.at("psi").at(kind);
                            const auto pressure_path = path + ".memory.psi." + kind;
                            if (!pressure.is_object()) {
                                add_error(errors, line_number, pressure_path,
                                          EvidenceValidationErrorCode::wrong_type,
                                          "Expected an object or null.");
                                continue;
                            }
                            exact_keys(pressure,
                                       {"average_10_seconds", "average_60_seconds",
                                        "average_300_seconds", "total_microseconds"},
                                       pressure_path, line_number, errors);
                            for (const auto average : {"average_10_seconds", "average_60_seconds",
                                                       "average_300_seconds"})
                                if (pressure.contains(average) &&
                                    (!pressure.at(average).is_number() ||
                                     pressure.at(average).get<double>() < 0.0 ||
                                     pressure.at(average).get<double>() > 100.0))
                                    add_error(errors, line_number, pressure_path + "." + average,
                                              EvidenceValidationErrorCode::invalid_value,
                                              "Expected a PSI average from 0 through 100.");
                            if (pressure.contains("total_microseconds"))
                                metric_or_null(pressure.at("total_microseconds"),
                                               pressure_path + ".total_microseconds", line_number,
                                               errors);
                        }
                    }
                    if (memory.contains("unavailable_metrics") &&
                        memory.at("unavailable_metrics").is_array())
                        for (std::size_t metric_index{};
                             metric_index < memory.at("unavailable_metrics").size(); ++metric_index)
                            diagnostic(memory.at("unavailable_metrics").at(metric_index),
                                       path + ".memory.unavailable_metrics[" +
                                           std::to_string(metric_index) + "]",
                                       line_number, errors);
                    if (memory.contains("unavailable_metrics") &&
                        !memory.at("unavailable_metrics").is_array())
                        add_error(errors, line_number, path + ".memory.unavailable_metrics",
                                  EvidenceValidationErrorCode::wrong_type, "Expected an array.");
                }
                if (container.contains("assessment") && !container.at("assessment").is_object())
                    add_error(errors, line_number, path + ".assessment",
                              EvidenceValidationErrorCode::wrong_type, "Expected an object.");
                if (container.contains("assessment") && container.at("assessment").is_object()) {
                    const auto &assessment = container.at("assessment");
                    exact_keys(assessment,
                               {"overall_risk", "coverage", "findings", "unavailable_evidence"},
                               path + ".assessment", line_number, errors);
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
                    if (assessment.contains("unavailable_evidence") &&
                        assessment.at("unavailable_evidence").is_array())
                        for (std::size_t evidence_index{};
                             evidence_index < assessment.at("unavailable_evidence").size();
                             ++evidence_index)
                            diagnostic(assessment.at("unavailable_evidence").at(evidence_index),
                                       path + ".assessment.unavailable_evidence[" +
                                           std::to_string(evidence_index) + "]",
                                       line_number, errors);
                    if (assessment.contains("unavailable_evidence") &&
                        !assessment.at("unavailable_evidence").is_array())
                        add_error(errors, line_number, path + ".assessment.unavailable_evidence",
                                  EvidenceValidationErrorCode::wrong_type, "Expected an array.");
                    if (assessment.contains("findings") && !assessment.at("findings").is_array())
                        add_error(errors, line_number, path + ".assessment.findings",
                                  EvidenceValidationErrorCode::wrong_type, "Expected an array.");
                    if (assessment.contains("findings") && assessment.at("findings").is_array())
                        for (std::size_t finding_index{};
                             finding_index < assessment.at("findings").size(); ++finding_index) {
                            const auto &finding = assessment.at("findings").at(finding_index);
                            const auto finding_path = path + ".assessment.findings[" +
                                                      std::to_string(finding_index) + "]";
                            if (!finding.is_object()) {
                                add_error(errors, line_number, finding_path,
                                          EvidenceValidationErrorCode::wrong_type,
                                          "Expected a finding object.");
                                continue;
                            }
                            exact_keys(finding, {"code", "risk", "evidence", "recommendation"},
                                       finding_path, line_number, errors);
                            for (const auto key : {"code", "risk", "recommendation"})
                                if (finding.contains(key) && !finding.at(key).is_string())
                                    add_error(errors, line_number, finding_path + "." + key,
                                              EvidenceValidationErrorCode::wrong_type,
                                              "Expected a string.");
                            if (finding.contains("risk") && finding.at("risk").is_string() &&
                                finding.at("risk") != "low" && finding.at("risk") != "moderate" &&
                                finding.at("risk") != "high" && finding.at("risk") != "critical")
                                add_error(errors, line_number, finding_path + ".risk",
                                          EvidenceValidationErrorCode::invalid_value,
                                          "Expected a known risk level.");
                            if (finding.contains("evidence") && !finding.at("evidence").is_array())
                                add_error(errors, line_number, finding_path + ".evidence",
                                          EvidenceValidationErrorCode::wrong_type,
                                          "Expected an array.");
                            if (finding.contains("evidence") && finding.at("evidence").is_array())
                                for (std::size_t evidence_index{};
                                     evidence_index < finding.at("evidence").size();
                                     ++evidence_index) {
                                    const auto &evidence =
                                        finding.at("evidence").at(evidence_index);
                                    const auto evidence_path = finding_path + ".evidence[" +
                                                               std::to_string(evidence_index) + "]";
                                    if (!evidence.is_object()) {
                                        add_error(errors, line_number, evidence_path,
                                                  EvidenceValidationErrorCode::wrong_type,
                                                  "Expected an evidence object.");
                                        continue;
                                    }
                                    exact_keys(evidence, {"code", "value"}, evidence_path,
                                               line_number, errors);
                                    if (evidence.contains("code") &&
                                        !evidence.at("code").is_string())
                                        add_error(errors, line_number, evidence_path + ".code",
                                                  EvidenceValidationErrorCode::wrong_type,
                                                  "Expected a string.");
                                    if (evidence.contains("value") &&
                                        evidence.at("value").is_null())
                                        add_error(errors, line_number, evidence_path + ".value",
                                                  EvidenceValidationErrorCode::wrong_type,
                                                  "Expected a decimal string.");
                                    else if (evidence.contains("value"))
                                        metric_or_null(evidence.at("value"),
                                                       evidence_path + ".value", line_number,
                                                       errors);
                                }
                        }
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
