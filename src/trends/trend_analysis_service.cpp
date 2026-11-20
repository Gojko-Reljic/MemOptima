#include <memoptima/trends/trend_analysis_service.hpp>

#include "trend_analysis_replay.hpp"
#include "typed_evidence_artifact_reader.hpp"
#include "utc_timestamp_parser.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

namespace memoptima::trends {
namespace {

[[nodiscard]] std::unexpected<TrendAnalysisError>
service_error(TrendAnalysisErrorCode code, std::optional<std::filesystem::path> artifact_path,
              std::string detail) {
    return std::unexpected(TrendAnalysisError{
        .code = code,
        .artifact_path = std::move(artifact_path),
        .detail = std::move(detail),
    });
}

[[nodiscard]] TrendMemoryMaximum
to_trend_maximum(const memoptima::detail::TypedEvidenceMemoryMaximum &maximum) {
    using SourceKind = memoptima::detail::TypedEvidenceMemoryMaximumKind;
    using TargetKind = TrendMemoryMaximumKind;
    switch (maximum.kind) {
    case SourceKind::finite:
        return {.kind = TargetKind::finite, .bytes = maximum.bytes};
    case SourceKind::unlimited:
        return {.kind = TargetKind::unlimited};
    case SourceKind::unavailable:
        return {.kind = TargetKind::unavailable};
    }
    return {.kind = TargetKind::unavailable};
}

class ReplaySink final : public memoptima::detail::TypedEvidenceArtifactSink {
  public:
    explicit ReplaySink(TrendAnalyzer &analyzer) : analyzer_{analyzer} {}

    std::expected<void, memoptima::detail::EvidenceArtifactTraversalError>
    consume(const memoptima::detail::TypedEvidenceSessionStart &start) override {
        metadata_.started_at_utc =
            memoptima::detail::format_canonical_utc_timestamp(start.started_at);
        metadata_.requested_interval_ms = start.requested_interval_ms;
        return forward(
            analyzer_.begin_session({.started_at = start.started_at,
                                     .requested_interval_ms = start.requested_interval_ms}));
    }

    std::expected<void, memoptima::detail::EvidenceArtifactTraversalError>
    consume(const memoptima::detail::TypedEvidenceSample &sample) override {
        const auto begin = forward(analyzer_.begin_sample({.captured_at = sample.captured_at}));
        if (!begin.has_value()) {
            return begin;
        }

        const std::string captured_at =
            memoptima::detail::format_canonical_utc_timestamp(sample.captured_at);
        for (const auto &container : sample.containers) {
            const TrendContainerIdentity identity{
                .container_id = container.container_id.value(),
                .cgroup_path = container.cgroup_path.string(),
            };
            const auto window = update_window(identity, captured_at);
            if (!window.has_value()) {
                return window;
            }
            const auto observed = forward(
                analyzer_.observe({.identity = identity,
                                   .current_memory_bytes = container.current_memory_bytes,
                                   .maximum_memory = to_trend_maximum(container.maximum_memory)}));
            if (!observed.has_value()) {
                return observed;
            }
        }
        return forward(analyzer_.end_sample());
    }

    std::expected<void, memoptima::detail::EvidenceArtifactTraversalError>
    consume(const memoptima::detail::TypedEvidenceSessionEnd &) override {
        return {};
    }

    [[nodiscard]] const std::optional<TrendAnalyzerError> &analyzer_error() const {
        return analyzer_error_;
    }

    [[nodiscard]] detail::TrendReplaySessionMetadata finish(bool incomplete) && {
        metadata_.incomplete = incomplete;
        for (auto &[identity, window] : windows_) {
            (void)identity;
            metadata_.container_windows.push_back(std::move(window));
        }
        return std::move(metadata_);
    }

  private:
    std::expected<void, memoptima::detail::EvidenceArtifactTraversalError>
    forward(std::expected<void, TrendAnalyzerError> result) {
        if (result.has_value()) {
            return {};
        }
        analyzer_error_ = result.error();
        return std::unexpected(memoptima::detail::EvidenceArtifactTraversalError{
            .detail = result.error().detail,
        });
    }

    std::expected<void, memoptima::detail::EvidenceArtifactTraversalError>
    update_window(const TrendContainerIdentity &identity, const std::string &captured_at) {
        if (!windows_.contains(identity) &&
            windows_.size() >= kTrendEvidenceCatalogMaximumContainerWindows) {
            return std::unexpected(memoptima::detail::EvidenceArtifactTraversalError{
                .detail = "The replayed artifact exceeds the configured container-window limit.",
            });
        }
        auto [position, inserted] = windows_.try_emplace(
            identity, TrendContainerObservationWindow{.identity = identity,
                                                      .minimum_captured_at_utc = captured_at,
                                                      .maximum_captured_at_utc = captured_at});
        if (!inserted) {
            position->second.minimum_captured_at_utc =
                std::min(position->second.minimum_captured_at_utc, captured_at);
            position->second.maximum_captured_at_utc =
                std::max(position->second.maximum_captured_at_utc, captured_at);
        }
        return {};
    }

    TrendAnalyzer &analyzer_;
    detail::TrendReplaySessionMetadata metadata_;
    std::map<TrendContainerIdentity, TrendContainerObservationWindow> windows_;
    std::optional<TrendAnalyzerError> analyzer_error_;
};

[[nodiscard]] std::expected<void, TrendAnalysisError>
replay_session(const TrendEvidenceSession &session, TrendAnalyzer &analyzer) {
    ReplaySink sink{analyzer};
    const auto traversal =
        memoptima::detail::traverse_typed_evidence_artifact(session.artifact_path, sink);
    if (sink.analyzer_error().has_value()) {
        return service_error(TrendAnalysisErrorCode::analyzer_failed, session.artifact_path,
                             sink.analyzer_error()->detail);
    }
    if (!traversal.has_value()) {
        return service_error(TrendAnalysisErrorCode::evidence_replay_failed, session.artifact_path,
                             traversal.error().detail);
    }

    // Cataloging and replay are separate streaming passes. Require their selection metadata to
    // agree so an artifact changed between passes cannot silently alter the analysis.
    const auto replay_metadata = std::move(sink).finish(traversal->incomplete);
    if (const auto mismatch = detail::replay_metadata_mismatch(session, replay_metadata)) {
        return service_error(TrendAnalysisErrorCode::evidence_replay_failed, session.artifact_path,
                             *mismatch);
    }

    const auto ended = analyzer.end_session({.incomplete = traversal->incomplete});
    if (!ended.has_value()) {
        return service_error(TrendAnalysisErrorCode::analyzer_failed, session.artifact_path,
                             ended.error().detail);
    }
    return {};
}

} // namespace

namespace detail {

std::optional<std::string>
replay_metadata_mismatch(const TrendEvidenceSession &catalog_session,
                         const TrendReplaySessionMetadata &replay_metadata) {
    if (catalog_session.started_at_utc != replay_metadata.started_at_utc) {
        return "The replayed session start timestamp differs from the catalog.";
    }
    if (catalog_session.requested_interval_ms != replay_metadata.requested_interval_ms) {
        return "The replayed requested interval differs from the catalog.";
    }
    if (catalog_session.incomplete != replay_metadata.incomplete) {
        return "The replayed completion state differs from the catalog.";
    }

    std::map<TrendContainerIdentity, TrendContainerObservationWindow> catalog_windows;
    for (const auto &window : catalog_session.container_windows) {
        catalog_windows.emplace(window.identity, window);
    }
    std::map<TrendContainerIdentity, TrendContainerObservationWindow> replay_windows;
    for (const auto &window : replay_metadata.container_windows) {
        replay_windows.emplace(window.identity, window);
    }
    if (catalog_windows.size() != replay_windows.size()) {
        return "The replayed container identity set differs from the catalog.";
    }
    for (const auto &[identity, catalog_window] : catalog_windows) {
        const auto replay = replay_windows.find(identity);
        if (replay == replay_windows.end()) {
            return "The replayed container identity set differs from the catalog.";
        }
        if (catalog_window.minimum_captured_at_utc != replay->second.minimum_captured_at_utc ||
            catalog_window.maximum_captured_at_utc != replay->second.maximum_captured_at_utc) {
            return "The replayed container observation window differs from the catalog.";
        }
    }
    return std::nullopt;
}

std::expected<void, TrendAnalysisError>
replay_trend_evidence_session(const TrendEvidenceSession &session, TrendAnalyzer &analyzer) {
    return replay_session(session, analyzer);
}

} // namespace detail

std::expected<TrendAnalysis, TrendAnalysisError>
analyze_trend_manifest(const TrendManifest &manifest) {
    const auto catalog = load_trend_evidence_catalog(manifest);
    if (!catalog.has_value()) {
        return service_error(TrendAnalysisErrorCode::catalog_failed, catalog.error().artifact_path,
                             catalog.error().detail);
    }

    TrendAnalyzer analyzer;
    for (const auto &session : catalog->sessions) {
        const auto replayed = detail::replay_trend_evidence_session(session, analyzer);
        if (!replayed.has_value()) {
            return std::unexpected(replayed.error());
        }
    }

    const auto analysis = analyzer.finish();
    if (!analysis.has_value()) {
        return service_error(TrendAnalysisErrorCode::analyzer_failed, std::nullopt,
                             analysis.error().detail);
    }
    return *analysis;
}

} // namespace memoptima::trends
