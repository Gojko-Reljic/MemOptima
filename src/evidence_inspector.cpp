#include <memoptima/evidence_inspector.hpp>

#include <memoptima/evidence_input_reader.hpp>
#include <memoptima/evidence_record_parser.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

namespace memoptima {
namespace {
using Json = nlohmann::ordered_json;

int risk_rank(const std::string &risk) {
    if (risk == "critical")
        return 3;
    if (risk == "high")
        return 2;
    if (risk == "moderate")
        return 1;
    return 0;
}

struct Aggregate {
    std::uint32_t samples_seen{};
    std::string highest_risk{"low"};
    std::uint32_t partial_coverage_samples{};
    std::string last_risk;
    std::string last_coverage;
};

std::string format_validation_errors(const std::vector<EvidenceValidationError> &errors) {
    std::string detail;
    for (const auto &error : errors) {
        if (!detail.empty())
            detail += '\n';
        detail += "Line " + std::to_string(error.line_number) + " at " + error.json_path + ": " +
                  error.detail;
    }
    return detail;
}
} // namespace

std::expected<EvidenceInspectionSummary, EvidenceInspectionError>
inspect_evidence_artifact(const std::filesystem::path &path) {
    auto input = EvidenceInputReader::open(path);
    if (!input.has_value())
        return std::unexpected(EvidenceInspectionError{input.error().detail});
    EvidenceInspectionSummary summary{.artifact_path = path,
                                      .application_version = {},
                                      .requested_sample_count = 0,
                                      .completed_sample_count = 0,
                                      .discovery_warning_count = 0,
                                      .skipped_container_count = 0,
                                      .incomplete = false,
                                      .terminal_outcome = {},
                                      .containers = {}};
    std::map<std::string, Aggregate> aggregates;
    bool saw_start{};
    bool saw_end{};
    std::uint32_t expected_sequence = 1;
    std::uint64_t line_number{};
    std::vector<EvidenceValidationError> validation_errors;
    while (true) {
        const auto line = input->next_line();
        if (!line.has_value())
            return std::unexpected(EvidenceInspectionError{line.error().detail});
        if (!line->has_value())
            break;
        ++line_number;
        const auto record = parse_evidence_record(**line, line_number);
        if (!record.has_value()) {
            for (const auto &error : record.error()) {
                if (validation_errors.size() == 20)
                    break;
                validation_errors.push_back(error);
            }
            if (validation_errors.size() == 20)
                break;
            continue;
        }
        if (saw_end || (!saw_start && record->type != EvidenceRecordType::session_start) ||
            (saw_start && record->type == EvidenceRecordType::session_start))
            return std::unexpected(EvidenceInspectionError{"Line " + std::to_string(line_number) +
                                                           ": invalid record order."});
        const Json value = Json::parse(record->canonical_json);
        if (record->type == EvidenceRecordType::session_start) {
            saw_start = true;
            summary.application_version = value.at("application_version").get<std::string>();
            summary.requested_sample_count =
                value.at("requested_sample_count").get<std::uint32_t>();
        } else if (record->type == EvidenceRecordType::sample) {
            if (value.at("sample_sequence").get<std::uint32_t>() != expected_sequence++)
                return std::unexpected(
                    EvidenceInspectionError{"Line " + std::to_string(line_number) +
                                            ": sample sequences must be contiguous."});
            ++summary.completed_sample_count;
            summary.discovery_warning_count += value.at("discovery_warnings").size();
            summary.skipped_container_count +=
                std::stoull(value.at("skipped_container_count").get<std::string>());
            for (const auto &container : value.at("containers")) {
                const auto id = container.at("container_id").get<std::string>();
                const auto risk = container.at("assessment").at("overall_risk").get<std::string>();
                const auto coverage = container.at("assessment").at("coverage").get<std::string>();
                auto &aggregate = aggregates[id];
                ++aggregate.samples_seen;
                if (risk_rank(risk) > risk_rank(aggregate.highest_risk))
                    aggregate.highest_risk = risk;
                if (coverage == "partial")
                    ++aggregate.partial_coverage_samples;
                aggregate.last_risk = risk;
                aggregate.last_coverage = coverage;
            }
        } else {
            saw_end = true;
            summary.terminal_outcome = value.at("outcome").get<std::string>();
            if (value.at("completed_sample_count").get<std::uint32_t>() !=
                summary.completed_sample_count)
                return std::unexpected(
                    EvidenceInspectionError{"Line " + std::to_string(line_number) +
                                            ": terminal sample count does not match the records."});
        }
    }
    if (!validation_errors.empty())
        return std::unexpected(
            EvidenceInspectionError{format_validation_errors(validation_errors)});
    if (!saw_start)
        return std::unexpected(
            EvidenceInspectionError{"The artifact has no session_start record."});
    summary.incomplete = !saw_end;
    for (const auto &[id, aggregate] : aggregates)
        summary.containers.push_back({id, aggregate.samples_seen, aggregate.highest_risk,
                                      aggregate.partial_coverage_samples, aggregate.last_risk,
                                      aggregate.last_coverage});
    return summary;
}
} // namespace memoptima
