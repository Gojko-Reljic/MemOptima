#include <memoptima/evidence/evidence_artifact_stream.hpp>

#include "typed_evidence_artifact_reader.hpp"
#include "utc_timestamp_parser.hpp"

#include <utility>

namespace memoptima::evidence {
namespace {

[[nodiscard]] ArtifactMemoryMaximum
to_public_maximum(const detail::TypedEvidenceMemoryMaximum &maximum) {
    using SourceKind = detail::TypedEvidenceMemoryMaximumKind;
    using TargetKind = ArtifactMemoryMaximumKind;
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

[[nodiscard]] ArtifactSessionOutcome to_public_outcome(sampling::SamplingSessionOutcome outcome) {
    switch (outcome) {
    case sampling::SamplingSessionOutcome::completed:
        return ArtifactSessionOutcome::completed;
    case sampling::SamplingSessionOutcome::interrupted:
        return ArtifactSessionOutcome::interrupted;
    case sampling::SamplingSessionOutcome::failed:
        return ArtifactSessionOutcome::failed;
    }
    return ArtifactSessionOutcome::failed;
}

class PublicArtifactSinkAdapter final : public detail::TypedEvidenceArtifactSink {
  public:
    explicit PublicArtifactSinkAdapter(ArtifactSink &sink) : sink_{sink} {}

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSessionStart &start) override {
        return forward(sink_.consume(
            {.line_number = start.line_number,
             .started_at = start.started_at,
             .started_at_utc = detail::format_canonical_utc_timestamp(start.started_at),
             .requested_sample_count = start.requested_sample_count,
             .requested_interval_ms = start.requested_interval_ms}));
    }

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSample &sample) override {
        ArtifactSample public_sample{.line_number = sample.line_number,
                                     .captured_at = sample.captured_at,
                                     .captured_at_utc =
                                         detail::format_canonical_utc_timestamp(sample.captured_at),
                                     .containers = {}};
        public_sample.containers.reserve(sample.containers.size());
        for (const auto &container : sample.containers) {
            public_sample.containers.push_back(
                {.container_id = container.container_id.value(),
                 .cgroup_path = container.cgroup_path.string(),
                 .current_memory_bytes = container.current_memory_bytes,
                 .maximum_memory = to_public_maximum(container.maximum_memory)});
        }
        return forward(sink_.consume(public_sample));
    }

    std::expected<void, detail::EvidenceArtifactTraversalError>
    consume(const detail::TypedEvidenceSessionEnd &end) override {
        return forward(sink_.consume({.line_number = end.line_number,
                                      .outcome = to_public_outcome(end.outcome),
                                      .ended_at = end.ended_at}));
    }

  private:
    [[nodiscard]] static std::expected<void, detail::EvidenceArtifactTraversalError>
    forward(std::expected<void, ArtifactTraversalError> result) {
        if (result.has_value()) {
            return {};
        }
        return std::unexpected(
            detail::EvidenceArtifactTraversalError{.detail = std::move(result.error().detail)});
    }

    ArtifactSink &sink_;
};

} // namespace

std::expected<ArtifactTraversalResult, ArtifactTraversalError>
traverse_artifact_beneath(const std::filesystem::path &root,
                          const std::filesystem::path &relative_path, ArtifactSink &sink) {
    PublicArtifactSinkAdapter adapter{sink};
    const auto traversal =
        detail::traverse_typed_evidence_artifact_beneath(root, relative_path, adapter);
    if (!traversal.has_value()) {
        return std::unexpected(ArtifactTraversalError{.detail = traversal.error().detail});
    }
    return ArtifactTraversalResult{.incomplete = traversal->incomplete};
}

} // namespace memoptima::evidence
