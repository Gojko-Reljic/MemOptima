#include <memoptima/trends/evidence_session_catalog.hpp>

#include "trend_artifact_path.hpp"
#include "typed_evidence_artifact_reader.hpp"
#include "utc_timestamp_parser.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace memoptima::trends {
namespace {

[[nodiscard]] std::unexpected<TrendEvidenceCatalogError>
catalog_error(TrendEvidenceCatalogErrorCode code, const std::filesystem::path &artifact_path,
              std::string detail) {
    return std::unexpected(TrendEvidenceCatalogError{
        .code = code,
        .artifact_path = artifact_path,
        .detail = std::move(detail),
    });
}

class CatalogSink final : public detail::TypedEvidenceArtifactSink {
  public:
    explicit CatalogSink(const std::filesystem::path &artifact_path)
        : artifact_path_{artifact_path} {}

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSessionStart &start) override {
        session_.artifact_path = artifact_path_;
        session_.started_at_utc = detail::format_canonical_utc_timestamp(start.started_at);
        session_.requested_interval_ms = start.requested_interval_ms;
        return {};
    }

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSample &sample) override {
        const std::string captured_at = detail::format_canonical_utc_timestamp(sample.captured_at);
        for (const auto &container : sample.containers) {
            TrendContainerIdentity identity{
                .container_id = container.container_id.value(),
                .cgroup_path = container.cgroup_path.string(),
            };
            auto [position, inserted] = windows_.try_emplace(
                std::move(identity),
                TrendContainerObservationWindow{.identity = {},
                                                .minimum_captured_at_utc = captured_at,
                                                .maximum_captured_at_utc = captured_at});
            if (inserted) {
                position->second.identity = position->first;
            } else {
                position->second.minimum_captured_at_utc =
                    std::min(position->second.minimum_captured_at_utc, captured_at);
                position->second.maximum_captured_at_utc =
                    std::max(position->second.maximum_captured_at_utc, captured_at);
            }
            if (windows_.size() > kTrendEvidenceCatalogMaximumContainerWindows) {
                return std::unexpected(detail::EvidenceArtifactTraversalError{
                    .detail = "Line " + std::to_string(sample.line_number) +
                              ": the artifact exceeds the configured container-window limit.",
                });
            }
        }
        return {};
    }

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSessionEnd &) override {
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
load_session(const std::filesystem::path &manifest_directory,
             const std::filesystem::path &artifact_path) {
    const auto relative_path =
        trend_path::relative_artifact_path(manifest_directory, artifact_path);
    if (!relative_path.has_value()) {
        return catalog_error(TrendEvidenceCatalogErrorCode::artifact_validation_failed,
                             artifact_path, "The artifact path is outside the manifest directory.");
    }
    CatalogSink sink{artifact_path};
    const auto traversal =
        detail::traverse_typed_evidence_artifact_beneath(manifest_directory, *relative_path, sink);
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
    if (manifest.artifact_paths.size() > kTrendManifestMaximumArtifacts) {
        return catalog_error(TrendEvidenceCatalogErrorCode::resource_limit_exceeded, {},
                             "The manifest exceeds the configured artifact limit.");
    }

    TrendEvidenceCatalog catalog;
    catalog.sessions.reserve(manifest.artifact_paths.size());
    for (const auto &artifact_path : manifest.artifact_paths) {
        auto session = load_session(manifest.directory, artifact_path);
        if (!session.has_value()) {
            return std::unexpected(session.error());
        }
        catalog.sessions.push_back(std::move(*session));
    }

    // A path tie-breaker makes catalog and replay order reproducible when sessions start together.
    std::ranges::sort(catalog.sessions,
                      [](const TrendEvidenceSession &left, const TrendEvidenceSession &right) {
                          if (left.started_at_utc != right.started_at_utc) {
                              return left.started_at_utc < right.started_at_utc;
                          }
                          return left.artifact_path.native() < right.artifact_path.native();
                      });

    // Overlapping windows would make one logical timeline ambiguous, so reject them before replay.
    std::map<TrendContainerIdentity, std::string> observed_through;
    for (const auto &session : catalog.sessions) {
        for (const auto &window : session.container_windows) {
            const auto prior = observed_through.find(window.identity);
            if (prior != observed_through.end() &&
                window.minimum_captured_at_utc <= prior->second) {
                return catalog_error(
                    TrendEvidenceCatalogErrorCode::ambiguous_session_overlap, session.artifact_path,
                    "The artifact overlaps a previously selected session for container '" +
                        window.identity.container_id + "' and cgroup path '" +
                        window.identity.cgroup_path + "'.");
            }
            if (prior == observed_through.end()) {
                observed_through.emplace(window.identity, window.maximum_captured_at_utc);
            } else {
                prior->second = std::max(prior->second, window.maximum_captured_at_utc);
            }
        }
    }
    return catalog;
}

} // namespace memoptima::trends
