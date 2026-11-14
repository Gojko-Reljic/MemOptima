#include <memoptima/evidence_inspector.hpp>

#include "evidence_artifact_reader.hpp"

#include <nlohmann/json.hpp>

#include <charconv>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>

namespace memoptima {
namespace {

using Json = nlohmann::ordered_json;

int risk_rank(const std::string &risk) {
    if (risk == "critical") {
        return 3;
    }
    if (risk == "high") {
        return 2;
    }
    if (risk == "moderate") {
        return 1;
    }
    return 0;
}

struct Aggregate {
    std::uint32_t samples_seen{};
    std::string highest_risk{"low"};
    std::uint32_t partial_coverage_samples{};
    std::string last_risk;
    std::string last_coverage;
};

bool parse_decimal(std::string_view text, std::uint64_t &value) {
    const auto [position, conversion_error] =
        std::from_chars(text.data(), text.data() + text.size(), value);
    return conversion_error == std::errc{} && position == text.data() + text.size();
}

class InspectionSink final : public detail::EvidenceArtifactRecordSink {
  public:
    explicit InspectionSink(const std::filesystem::path &artifact_path)
        : summary_{.artifact_path = artifact_path,
                   .schema_version = 1,
                   .application_version = {},
                   .requested_sample_count = 0,
                   .requested_interval_ms = {},
                   .completed_sample_count = 0,
                   .first_capture_at_utc = {},
                   .last_capture_at_utc = {},
                   .discovery_warning_count = 0,
                   .skipped_container_count = 0,
                   .incomplete = false,
                   .terminal_outcome = {},
                   .containers = {}} {}

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::EvidenceArtifactRecord &record) override {
        const Json value = Json::parse(record.canonical_json);
        if (record.type == EvidenceRecordType::session_start) {
            summary_.application_version = value.at("application_version").get<std::string>();
            summary_.requested_sample_count =
                value.at("requested_sample_count").get<std::uint32_t>();
            summary_.requested_interval_ms = value.at("requested_interval_ms").get<std::string>();
            return {};
        }
        if (record.type == EvidenceRecordType::session_end) {
            summary_.terminal_outcome = value.at("outcome").get<std::string>();
            return {};
        }

        ++summary_.completed_sample_count;
        const auto captured_at = value.at("captured_at_utc").get<std::string>();
        if (summary_.first_capture_at_utc.empty()) {
            summary_.first_capture_at_utc = captured_at;
        }
        summary_.last_capture_at_utc = captured_at;
        summary_.discovery_warning_count += value.at("discovery_warnings").size();

        std::uint64_t skipped{};
        if (!parse_decimal(value.at("skipped_container_count").get<std::string>(), skipped) ||
            skipped >
                std::numeric_limits<std::uint64_t>::max() - summary_.skipped_container_count) {
            return failure(record.line_number,
                           "skipped container count cannot be aggregated safely.");
        }
        summary_.skipped_container_count += skipped;

        std::set<std::string> sample_container_ids;
        for (const auto &container : value.at("containers")) {
            const auto id = container.at("container_id").get<std::string>();
            if (!sample_container_ids.insert(id).second) {
                return failure(record.line_number,
                               "a container ID occurs more than once in one sample.");
            }
            const auto risk = container.at("assessment").at("overall_risk").get<std::string>();
            const auto coverage = container.at("assessment").at("coverage").get<std::string>();
            auto &aggregate = aggregates_[id];
            ++aggregate.samples_seen;
            if (risk_rank(risk) > risk_rank(aggregate.highest_risk)) {
                aggregate.highest_risk = risk;
            }
            if (coverage == "partial") {
                ++aggregate.partial_coverage_samples;
            }
            aggregate.last_risk = risk;
            aggregate.last_coverage = coverage;
        }
        return {};
    }

    [[nodiscard]] EvidenceInspectionSummary finish(bool incomplete) {
        summary_.incomplete = incomplete;
        for (const auto &[id, aggregate] : aggregates_) {
            summary_.containers.push_back({id, aggregate.samples_seen, aggregate.highest_risk,
                                           aggregate.partial_coverage_samples, aggregate.last_risk,
                                           aggregate.last_coverage});
        }
        return std::move(summary_);
    }

  private:
    [[nodiscard]] static std::unexpected<detail::EvidenceArtifactTraversalError>
    failure(std::uint64_t line_number, std::string detail) {
        return std::unexpected(detail::EvidenceArtifactTraversalError{
            .detail = "Line " + std::to_string(line_number) + ": " + std::move(detail),
        });
    }

    EvidenceInspectionSummary summary_;
    std::map<std::string, Aggregate> aggregates_;
};

} // namespace

std::expected<EvidenceInspectionSummary, EvidenceInspectionError>
inspect_evidence_artifact(const std::filesystem::path &path) {
    InspectionSink sink{path};
    const auto traversal = detail::traverse_evidence_artifact(path, sink);
    if (!traversal.has_value()) {
        return std::unexpected(EvidenceInspectionError{.detail = traversal.error().detail});
    }
    return sink.finish(traversal->incomplete);
}

} // namespace memoptima
