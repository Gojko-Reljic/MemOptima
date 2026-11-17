#include <memoptima/trends/evidence_session_catalog.hpp>

#include <memoptima/docker_container_memory_snapshot.hpp>

#include "evidence_artifact_reader.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <map>
#include <string>
#include <string_view>
#include <utility>

namespace memoptima::trends {
namespace {

using Json = nlohmann::ordered_json;

[[nodiscard]] std::unexpected<TrendEvidenceCatalogError>
catalog_error(TrendEvidenceCatalogErrorCode code, const std::filesystem::path &artifact_path,
              std::string detail) {
    return std::unexpected(TrendEvidenceCatalogError{
        .code = code,
        .artifact_path = artifact_path,
        .detail = std::move(detail),
    });
}

[[nodiscard]] std::expected<std::uint64_t, detail::EvidenceArtifactTraversalError>
parse_decimal(std::string_view text, std::uint64_t line_number, std::string_view field_name) {
    std::uint64_t value{};
    const auto [position, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || position != text.data() + text.size()) {
        return std::unexpected(detail::EvidenceArtifactTraversalError{
            .detail = "Line " + std::to_string(line_number) + ": " + std::string{field_name} +
                      " cannot be converted safely.",
        });
    }
    return value;
}

class CatalogSink final : public detail::EvidenceArtifactRecordSink {
  public:
    explicit CatalogSink(const std::filesystem::path &artifact_path)
        : artifact_path_{artifact_path} {}

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::EvidenceArtifactRecord &record) override {
        const Json value = Json::parse(record.canonical_json);
        if (record.type == EvidenceRecordType::session_start) {
            session_.artifact_path = artifact_path_;
            session_.started_at_utc = value.at("started_at_utc").get<std::string>();
            const auto interval =
                parse_decimal(value.at("requested_interval_ms").get<std::string>(),
                              record.line_number, "requested_interval_ms");
            if (!interval.has_value()) {
                return std::unexpected(interval.error());
            }
            session_.requested_interval_ms = *interval;
            return {};
        }
        if (record.type != EvidenceRecordType::sample) {
            return {};
        }

        const std::string captured_at = value.at("captured_at_utc").get<std::string>();
        for (const auto &container : value.at("containers")) {
            const auto container_id =
                DockerContainerId::parse(container.at("container_id").get<std::string>());
            if (!container_id.has_value()) {
                return std::unexpected(detail::EvidenceArtifactTraversalError{
                    .detail = "Line " + std::to_string(record.line_number) +
                              ": container_id is invalid: " + container_id.error().detail,
                });
            }
            TrendContainerIdentity identity{
                .container_id = container_id->value(),
                .cgroup_path = container.at("cgroup_path").get<std::string>(),
            };
            auto [position, inserted] = windows_.try_emplace(
                std::move(identity),
                TrendContainerObservationWindow{.identity = {},
                                                .first_captured_at_utc = captured_at,
                                                .last_captured_at_utc = captured_at});
            if (inserted) {
                position->second.identity = position->first;
            } else {
                position->second.first_captured_at_utc =
                    std::min(position->second.first_captured_at_utc, captured_at);
                position->second.last_captured_at_utc =
                    std::max(position->second.last_captured_at_utc, captured_at);
            }
            if (windows_.size() > kTrendEvidenceCatalogMaximumContainerWindows) {
                return std::unexpected(detail::EvidenceArtifactTraversalError{
                    .detail = "Line " + std::to_string(record.line_number) +
                              ": the artifact exceeds the configured container-window limit.",
                });
            }
        }
        return {};
    }

    [[nodiscard]] TrendEvidenceSession finish(bool incomplete) && {
        session_.incomplete = incomplete;
        for (auto &[identity, window] : windows_) {
            (void)identity;
            session_.container_windows.push_back(std::move(window));
        }
        return std::move(session_);
    }

  private:
    std::filesystem::path artifact_path_;
    TrendEvidenceSession session_;
    std::map<TrendContainerIdentity, TrendContainerObservationWindow> windows_;
};

[[nodiscard]] std::expected<TrendEvidenceSession, TrendEvidenceCatalogError>
load_session(const std::filesystem::path &artifact_path) {
    CatalogSink sink{artifact_path};
    const auto traversal = detail::traverse_evidence_artifact(artifact_path, sink);
    if (!traversal.has_value()) {
        const auto code =
            traversal.error().detail.find("container-window limit") != std::string::npos
                ? TrendEvidenceCatalogErrorCode::resource_limit_exceeded
                : TrendEvidenceCatalogErrorCode::artifact_validation_failed;
        return catalog_error(code, artifact_path, traversal.error().detail);
    }
    return std::move(sink).finish(traversal->incomplete);
}

} // namespace

std::expected<TrendEvidenceCatalog, TrendEvidenceCatalogError>
load_trend_evidence_catalog(const TrendManifest &manifest) {
    TrendEvidenceCatalog catalog;
    catalog.sessions.reserve(manifest.artifact_paths.size());
    for (const auto &artifact_path : manifest.artifact_paths) {
        auto session = load_session(artifact_path);
        if (!session.has_value()) {
            return std::unexpected(session.error());
        }
        catalog.sessions.push_back(std::move(*session));
    }

    std::ranges::sort(catalog.sessions,
                      [](const TrendEvidenceSession &left, const TrendEvidenceSession &right) {
                          if (left.started_at_utc != right.started_at_utc) {
                              return left.started_at_utc < right.started_at_utc;
                          }
                          return left.artifact_path.native() < right.artifact_path.native();
                      });

    std::map<TrendContainerIdentity, std::string> observed_through;
    for (const auto &session : catalog.sessions) {
        for (const auto &window : session.container_windows) {
            const auto prior = observed_through.find(window.identity);
            if (prior != observed_through.end() && window.first_captured_at_utc <= prior->second) {
                return catalog_error(
                    TrendEvidenceCatalogErrorCode::ambiguous_session_overlap, session.artifact_path,
                    "The artifact overlaps a previously selected session for container '" +
                        window.identity.container_id + "' and cgroup path '" +
                        window.identity.cgroup_path + "'.");
            }
            if (prior == observed_through.end()) {
                observed_through.emplace(window.identity, window.last_captured_at_utc);
            } else {
                prior->second = std::max(prior->second, window.last_captured_at_utc);
            }
        }
    }
    return catalog;
}

} // namespace memoptima::trends
