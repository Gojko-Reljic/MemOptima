#include "evidence_artifact_reader.hpp"

#include <memoptima/evidence_input_reader.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace memoptima::detail {
namespace {

using Json = nlohmann::ordered_json;

constexpr std::size_t kMaximumValidationErrors = 20;

[[nodiscard]] std::string
format_validation_errors(const std::vector<EvidenceValidationError> &errors) {
    std::string detail;
    for (const auto &error : errors) {
        if (!detail.empty()) {
            detail += '\n';
        }
        detail += "Line " + std::to_string(error.line_number) + " at " + error.json_path + ": " +
                  error.detail;
    }
    return detail;
}

[[nodiscard]] std::unexpected<EvidenceArtifactTraversalError> error(std::string detail) {
    return std::unexpected(EvidenceArtifactTraversalError{.detail = std::move(detail)});
}

} // namespace

std::expected<EvidenceArtifactTraversalResult, EvidenceArtifactTraversalError>
traverse_evidence_artifact(const std::filesystem::path &path, EvidenceArtifactRecordSink &sink) {
    auto input = EvidenceInputReader::open(path);
    if (!input.has_value()) {
        return error(input.error().detail);
    }

    bool saw_start{};
    bool saw_end{};
    std::uint32_t requested_sample_count{};
    std::uint32_t completed_sample_count{};
    std::uint32_t expected_sequence = 1;
    std::uint64_t line_number{};
    std::vector<EvidenceValidationError> validation_errors;
    while (true) {
        const auto line = input->next_line();
        if (!line.has_value()) {
            return error(line.error().detail);
        }
        if (!line->has_value()) {
            break;
        }
        ++line_number;

        const auto parsed = parse_evidence_record(**line, line_number);
        if (!parsed.has_value()) {
            // Bound diagnostics so one malformed artifact cannot create unbounded error output.
            for (const auto &validation_error : parsed.error()) {
                if (validation_errors.size() == kMaximumValidationErrors) {
                    break;
                }
                validation_errors.push_back(validation_error);
            }
            if (validation_errors.size() == kMaximumValidationErrors) {
                break;
            }
            continue;
        }

        if (saw_end || (!saw_start && parsed->type != EvidenceRecordType::session_start) ||
            (saw_start && parsed->type == EvidenceRecordType::session_start)) {
            return error("Line " + std::to_string(line_number) + ": invalid record order.");
        }

        const Json value = Json::parse(parsed->canonical_json);
        if (parsed->type == EvidenceRecordType::session_start) {
            saw_start = true;
            requested_sample_count = value.at("requested_sample_count").get<std::uint32_t>();
        } else if (parsed->type == EvidenceRecordType::sample) {
            if (value.at("sample_sequence").get<std::uint32_t>() != expected_sequence++) {
                return error("Line " + std::to_string(line_number) +
                             ": sample sequences must be contiguous.");
            }
            if (completed_sample_count == requested_sample_count) {
                return error("Line " + std::to_string(line_number) +
                             ": sample count exceeds the requested sampling contract.");
            }
            ++completed_sample_count;
        } else {
            saw_end = true;
            const std::uint32_t terminal_sample_count =
                value.at("completed_sample_count").get<std::uint32_t>();
            if (terminal_sample_count != completed_sample_count) {
                return error("Line " + std::to_string(line_number) +
                             ": terminal sample count does not match the records.");
            }
            if (value.at("outcome") == "completed" &&
                completed_sample_count != requested_sample_count) {
                return error("Line " + std::to_string(line_number) +
                             ": a completed session must contain every requested sample.");
            }
        }

        const auto consumed = sink.consume(EvidenceArtifactRecord{
            .line_number = line_number,
            .type = parsed->type,
            .canonical_json = parsed->canonical_json,
        });
        if (!consumed.has_value()) {
            return std::unexpected(consumed.error());
        }
    }

    if (!validation_errors.empty()) {
        return error(format_validation_errors(validation_errors));
    }
    if (!saw_start) {
        return error("The artifact has no session_start record.");
    }
    return EvidenceArtifactTraversalResult{.incomplete = !saw_end};
}

} // namespace memoptima::detail
