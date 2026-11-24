#include <memoptima/forecast_evaluation/forecast_evaluation_service.hpp>

#include <memoptima/evidence/evidence_artifact_stream.hpp>
#include <memoptima/trends/evidence_session_catalog.hpp>

#include <algorithm>
#include <map>
#include <utility>

namespace memoptima::forecast_evaluation {
namespace {

struct ReplaySessionMetadata {
    std::string started_at_utc;
    std::uint64_t requested_interval_ms{};
    bool incomplete{};
    std::vector<trends::TrendContainerObservationWindow> container_windows;
};

[[nodiscard]] std::unexpected<ForecastEvaluationError>
service_error(ForecastEvaluationErrorCode code, std::optional<std::filesystem::path> artifact_path,
              std::string detail) {
    return std::unexpected(ForecastEvaluationError{
        .code = code, .artifact_path = std::move(artifact_path), .detail = std::move(detail)});
}

[[nodiscard]] std::optional<std::filesystem::path>
relative_artifact_path(const std::filesystem::path &manifest_directory,
                       const std::filesystem::path &artifact_path) {
    const auto relative = artifact_path.lexically_relative(manifest_directory);
    if (relative.empty() || relative.is_absolute() || *relative.begin() == "..") {
        return std::nullopt;
    }
    return relative;
}

[[nodiscard]] std::optional<std::string>
replay_metadata_mismatch(const trends::TrendEvidenceSession &catalog_session,
                         const ReplaySessionMetadata &replay_metadata) {
    if (catalog_session.started_at_utc != replay_metadata.started_at_utc) {
        return "The replayed session start timestamp differs from the catalog.";
    }
    if (catalog_session.requested_interval_ms != replay_metadata.requested_interval_ms) {
        return "The replayed requested interval differs from the catalog.";
    }
    if (catalog_session.incomplete != replay_metadata.incomplete) {
        return "The replayed completion state differs from the catalog.";
    }

    std::map<trends::TrendContainerIdentity, trends::TrendContainerObservationWindow>
        catalog_windows;
    for (const auto &window : catalog_session.container_windows) {
        catalog_windows.emplace(window.identity, window);
    }
    std::map<trends::TrendContainerIdentity, trends::TrendContainerObservationWindow>
        replay_windows;
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

class ReplaySink final : public evidence::ArtifactSink {
  public:
    explicit ReplaySink(ForecastEvaluationAnalyzer &analyzer) : analyzer_{analyzer} {}

    std::expected<void, evidence::ArtifactTraversalError>
    consume(const evidence::ArtifactSessionStart &start) override {
        metadata_.started_at_utc = start.started_at_utc;
        metadata_.requested_interval_ms = start.requested_interval_ms;
        return forward(analyzer_.begin_session());
    }

    std::expected<void, evidence::ArtifactTraversalError>
    consume(const evidence::ArtifactSample &sample) override {
        const auto begin = forward(analyzer_.begin_sample({.captured_at = sample.captured_at}));
        if (!begin.has_value()) {
            return begin;
        }
        for (const auto &container : sample.containers) {
            const trends::TrendContainerIdentity trend_identity{
                .container_id = container.container_id,
                .cgroup_path = container.cgroup_path,
            };
            update_window(trend_identity, sample.captured_at_utc);
            const auto observed = forward(
                analyzer_.observe({.identity = {.container_id = container.container_id,
                                                .cgroup_path = container.cgroup_path},
                                   .current_memory_bytes = container.current_memory_bytes}));
            if (!observed.has_value()) {
                return observed;
            }
        }
        return forward(analyzer_.end_sample());
    }

    std::expected<void, evidence::ArtifactTraversalError>
    consume(const evidence::ArtifactSessionEnd &) override {
        return {};
    }

    [[nodiscard]] const std::optional<ForecastEvaluationAnalyzerError> &analyzer_error() const {
        return analyzer_error_;
    }

    [[nodiscard]] ReplaySessionMetadata finish(bool incomplete) && {
        metadata_.incomplete = incomplete;
        for (auto &[identity, window] : windows_) {
            (void)identity;
            metadata_.container_windows.push_back(std::move(window));
        }
        return std::move(metadata_);
    }

  private:
    void update_window(const trends::TrendContainerIdentity &identity,
                       const std::string &captured_at_utc) {
        auto [position, inserted] = windows_.try_emplace(
            identity,
            trends::TrendContainerObservationWindow{.identity = identity,
                                                    .minimum_captured_at_utc = captured_at_utc,
                                                    .maximum_captured_at_utc = captured_at_utc});
        if (!inserted) {
            position->second.minimum_captured_at_utc =
                std::min(position->second.minimum_captured_at_utc, captured_at_utc);
            position->second.maximum_captured_at_utc =
                std::max(position->second.maximum_captured_at_utc, captured_at_utc);
        }
    }

    std::expected<void, evidence::ArtifactTraversalError>
    forward(std::expected<void, ForecastEvaluationAnalyzerError> result) {
        if (result.has_value()) {
            return {};
        }
        analyzer_error_ = result.error();
        return std::unexpected(evidence::ArtifactTraversalError{.detail = result.error().detail});
    }

    ForecastEvaluationAnalyzer &analyzer_;
    ReplaySessionMetadata metadata_;
    std::map<trends::TrendContainerIdentity, trends::TrendContainerObservationWindow> windows_;
    std::optional<ForecastEvaluationAnalyzerError> analyzer_error_;
};

[[nodiscard]] std::expected<void, ForecastEvaluationError>
replay_session(const std::filesystem::path &manifest_directory,
               const trends::TrendEvidenceSession &session, ForecastEvaluationAnalyzer &analyzer) {
    const auto relative_path = relative_artifact_path(manifest_directory, session.artifact_path);
    if (!relative_path.has_value()) {
        return service_error(ForecastEvaluationErrorCode::evidence_replay_failed,
                             session.artifact_path,
                             "The artifact path is outside the manifest directory.");
    }
    ReplaySink sink{analyzer};
    const auto traversal =
        evidence::traverse_artifact_beneath(manifest_directory, *relative_path, sink);
    if (sink.analyzer_error().has_value()) {
        return service_error(ForecastEvaluationErrorCode::analyzer_failed, session.artifact_path,
                             sink.analyzer_error()->detail);
    }
    if (!traversal.has_value()) {
        return service_error(ForecastEvaluationErrorCode::evidence_replay_failed,
                             session.artifact_path, traversal.error().detail);
    }
    const auto metadata = std::move(sink).finish(traversal->incomplete);
    if (const auto mismatch = replay_metadata_mismatch(session, metadata)) {
        return service_error(ForecastEvaluationErrorCode::evidence_replay_failed,
                             session.artifact_path, *mismatch);
    }
    const auto ended = analyzer.end_session({.incomplete = traversal->incomplete});
    if (!ended.has_value()) {
        return service_error(ForecastEvaluationErrorCode::analyzer_failed, session.artifact_path,
                             ended.error().detail);
    }
    return {};
}

} // namespace

std::expected<ForecastEvaluationReport, ForecastEvaluationError>
evaluate_forecast_manifest(const trends::TrendManifest &manifest) {
    const auto catalog = trends::load_trend_evidence_catalog(manifest);
    if (!catalog.has_value()) {
        return service_error(ForecastEvaluationErrorCode::catalog_failed,
                             catalog.error().artifact_path, catalog.error().detail);
    }

    ForecastEvaluationAnalyzer analyzer;
    for (const auto &session : catalog->sessions) {
        const auto replayed = replay_session(manifest.directory, session, analyzer);
        if (!replayed.has_value()) {
            return std::unexpected(replayed.error());
        }
    }
    const auto report = analyzer.finish();
    if (!report.has_value()) {
        return service_error(ForecastEvaluationErrorCode::analyzer_failed, std::nullopt,
                             report.error().detail);
    }
    return *report;
}

} // namespace memoptima::forecast_evaluation
