# MemOptima Architecture Decisions

This log records owner-approved decisions that affect architecture, public contracts, or product
scope. New entries must include the alternatives considered and their consequences.

## ADR-001: C++23 is the core implementation language

- **Status:** Approved
- **Decision:** MemOptima core application code uses C++23.
- **Reason:** The project requires predictable native performance, direct Linux integration, strong
  type modeling, and existing C++23 code continuity.
- **Consequence:** New core capabilities must not introduce another required runtime language without
  explicit owner approval.

## ADR-002: Modular architecture precedes parallel implementation

- **Status:** Approved
- **Decision:** Define module interfaces and dependency direction before parallel feature work.
- **Alternatives considered:** Let workstreams modify shared code directly; postpone module rules.
- **Reason:** Shared implementation changes without interfaces create merge conflicts and weaken
  responsibility boundaries.
- **Consequence:** Worktrees and parallel tasks begin only after their inputs, outputs, and ownership
  boundaries are approved.

## ADR-003: Module dependency rule

- **Status:** Approved
- **Decision:** A module may depend only on public interfaces of lower-level modules, never on their
  internal implementation details.
- **Reason:** This keeps modules replaceable and independently testable.
- **Consequence:** Internal headers, JSON traversal, filesystem adapters, and CLI formatting are not
  cross-module contracts.

## ADR-004: Contract-changing work requires owner approval

- **Status:** Approved
- **Decision:** Any change to the evidence artifact format, CLI output contract, public API, or module
  boundary requires an entry in this log and explicit owner approval before implementation.
- **Reason:** MemOptima must remain predictable for users and future integrations.
- **Consequence:** Such work is designed and reviewed before coding begins.

## ADR-005: MemOptima 2.0 is a product vision

- **Status:** Approved
- **Decision:** “MemOptima 2.0 — Predictive Memory Intelligence” names the product vision, not an
  immediate executable version change.
- **Reason:** The current release line follows semantic versioning and is at v0.6.
- **Consequence:** Releases continue incrementally until a separately approved major-version change.

## ADR-006: v0.7 includes local cross-session trend analysis

- **Status:** Approved
- **Decision:** v0.7 will add local, read-only analysis of explicitly selected evidence artifacts
  across sessions.
- **Alternatives considered:** Limit v0.7 to single-session diagnostics; immediately add anomaly or
  leak claims.
- **Reason:** Cross-session evidence is necessary for meaningful long-term trends and later advisory
  intelligence, while preserving a bounded operational scope.
- **Consequence:** v0.7 must define observation epochs, comparability diagnostics, provenance, and
  tests before interpreting long-term growth.

## ADR-007: Cross-session trend input uses an explicit manifest

- **Status:** Approved
- **Decision:** v0.7 cross-session trend analysis accepts an explicit manifest that names the local
  evidence artifacts to analyze.
- **Alternatives considered:** Repeated CLI file arguments; automatic directory scanning.
- **Reason:** A manifest is reproducible, auditable, and suitable for later backtesting and dashboard
  use without relying on filesystem discovery rules.
- **Consequence:** The manifest has to receive a versioned, bounded, documented format before the
  trend command is implemented.

## ADR-008: Use strict container identity for v0.7 observation epochs

- **Status:** Approved
- **Decision:** A v0.7 cross-session observation epoch requires the same full Docker container ID and
  cgroup path in every comparable artifact.
- **Alternatives considered:** A user-provided logical workload identity; best-effort matching by
  names, prefixes, or cgroup patterns.
- **Reason:** Strict identity prevents a redeployed or unrelated container from being presented as a
  continuous memory trend.
- **Consequence:** Identity changes end an epoch and begin a new trend. A user-provided logical
  workload identity remains a separately approved future capability.

## ADR-009: Apply fixed v0.7 resource limits to cross-session analysis

- **Status:** Approved
- **Decision:** Cross-session analysis streams evidence input and accepts at most 128 manifest
  artifacts and 4,096 active container observation epochs.
- **Alternatives considered:** No new limits; fully configurable CLI limits.
- **Reason:** Fixed initial limits keep CPU and memory overhead predictable and prevent accidental or
  hostile input from producing silently incomplete results.
- **Consequence:** Exceeding a limit produces a clear resource-limit result. Configurable limits may
  be considered later, but remain bounded by separate hard caps.

## ADR-010: Provide table and versioned JSON trend results

- **Status:** Superseded by ADR-037
- **Decision:** The v0.7 trend command provides a human-readable table by default and a versioned
  JSON result format on request.
- **Alternatives considered:** Table only; JSON only.
- **Reason:** Operators need a readable terminal report, while the future dashboard, automation, and
  external integrations need a stable machine-readable contract.
- **Consequence:** The table and JSON formats must represent the same result categories. The JSON
  schema is a public contract requiring documentation and tests before implementation.

## ADR-011: Use a single versioned JSON trend manifest

- **Status:** Approved
- **Decision:** A cross-session input manifest is one versioned JSON document with a closed schema
  and an explicit artifact list.
- **Alternatives considered:** JSON Lines; an untyped text file containing paths.
- **Reason:** A JSON document supports strict validation, a stable manifest version, and later
  provenance fields without changing the fundamental input model.
- **Consequence:** The manifest format, path semantics, and validation diagnostics must be documented
  and tested as a public contract before implementation.

## ADR-012: Restrict artifact paths to the manifest directory tree

- **Status:** Approved
- **Decision:** Artifact paths are relative to the manifest directory and may not escape its directory
  tree through absolute paths or parent traversal.
- **Alternatives considered:** Allow relative and absolute paths; require absolute paths only.
- **Reason:** A self-contained trend set is portable, reproducible, and prevents a manifest from
  selecting arbitrary local files.
- **Consequence:** Users organize a manifest and its evidence below one directory. Input validation
  rejects absolute paths and paths that resolve outside that tree.

## ADR-013: Order trend artifacts by validated session start time

- **Status:** Approved
- **Decision:** The manifest selects artifacts, while MemOptima orders them by validated session
  start time. Equal timestamps use a stable path order and duplicate artifact references are errors.
- **Alternatives considered:** Treat manifest order as analytical order; require the user to provide
  an already chronological list.
- **Reason:** Analysis must not change because a user manually reordered the same artifact list.
- **Consequence:** Sorting uses only validated evidence metadata and does not override strict epoch
  continuity rules.

## ADR-014: Fail closed when a selected artifact cannot be processed

- **Status:** Approved
- **Decision:** Trend analysis produces no report when any artifact named by the manifest is
  unavailable, unsafe, malformed, or lifecycle-invalid.
- **Alternatives considered:** Silently skip invalid artifacts; provide a partial mode immediately.
- **Reason:** A trend must represent the exact selected evidence set and must not hide an omitted time
  period behind a seemingly complete result.
- **Consequence:** Diagnostics identify the offending path and reason. A future partial mode, if
  approved, must be explicit and visibly mark every affected result as partial.

## ADR-015: Keep manifest version 1 minimal and closed

- **Status:** Approved
- **Decision:** Manifest version 1 accepts only `manifest_version`, `artifacts`, and artifact `path`.
- **Alternatives considered:** Optional artifact labels; a free-form metadata object.
- **Reason:** The manifest selects evidence and should not introduce unvalidated annotations that
  appear authoritative or affect analysis.
- **Consequence:** Unknown fields are validation errors. Later metadata requires a separately
  approved manifest version or a strictly defined contract extension.

## ADR-016: Use the `trend manifest` CLI command

- **Status:** Superseded by ADR-037
- **Decision:** The public v0.7 command is `memoptima trend manifest <FILE>`. It produces a table by
  default and JSON when requested through `--format json`.
- **Alternatives considered:** `trends manifest <FILE>`; `analyze trends <FILE>`.
- **Reason:** The singular command follows the existing `forecast evidence` and `inspect evidence`
  command style while making the manifest input explicit.
- **Consequence:** Help text, exit behavior, table output, and JSON output form part of the public
  CLI contract and require tests.

## ADR-017: Report trends per container observation epoch

- **Status:** Approved
- **Decision:** Each v0.7 trend result represents exactly one container observation epoch. A global
  summary counts statuses but does not merge separate epochs.
- **Alternatives considered:** Aggregate all epochs for one container ID; provide only a global
  summary.
- **Reason:** Separate epochs may have gaps, redeployments, or changed conditions that cannot safely
  be represented as one continuous trend.
- **Consequence:** Table output may abbreviate IDs for readability, but JSON retains the full Docker
  ID and epoch context.

## ADR-018: Trend `memory.current` only in v0.7

- **Status:** Approved
- **Decision:** v0.7 derives trends only from `memory.current`. `memory.max` supplies limit context
  and eligibility, not a separate trend.
- **Alternatives considered:** Combine current memory, swap, PSI, and cache in one trend model;
  combine current memory and PSI.
- **Reason:** The existing forecast foundation already models current memory. The other signals need
  distinct aggregation and interpretation rules before they can support reliable conclusions.
- **Consequence:** PSI, swap, and cache remain retained evidence and are candidates for v0.8
  intelligence work.

## ADR-019: Represent time gaps as explicit trend-quality signals

- **Status:** Approved
- **Decision:** A time gap does not by itself end a strict-identity observation epoch. The result
  records the largest gap, marks the trend as sparse, and prohibits a time-to-limit estimate.
- **Alternatives considered:** End an epoch at every gap; silently accept gaps as continuous data.
- **Reason:** Cross-session analysis needs to retain long-term identity while making unobserved
  intervals explicit and preventing them from supporting an unsafe limit-crossing estimate.
- **Consequence:** Non-increasing timestamps remain epoch-breaking errors. Table and JSON output must
  distinguish continuous and sparse quality.

## ADR-020: Derive sparse gaps from adjacent sampling intervals

- **Status:** Approved
- **Decision:** A gap is sparse only when it exceeds twice the larger declared sampling interval of
  its adjacent observations.
- **Alternatives considered:** One fixed duration threshold; mark every artifact boundary as sparse.
- **Reason:** The threshold follows the evidence collection contract and tolerates expected jitter
  without masking a material unobserved period.
- **Consequence:** Trend processing reads the relevant sampling interval from validated evidence and
  records the largest observed gap for every epoch.

## ADR-021: Reuse v0.6 minimum evidence for current-memory rates

- **Status:** Approved
- **Decision:** A `memory.current` rate requires at least three observations across at least ten
  seconds with strictly increasing timestamps. A time-to-limit estimate additionally requires
  continuous quality and a stable finite `memory.max`.
- **Alternatives considered:** A new five-observation, sixty-second threshold; requiring a finite
  limit for every current-memory trend.
- **Reason:** The existing minimum is tested, and current-memory behavior remains useful even when a
  container has no finite hard limit.
- **Consequence:** Sparse or unlimited epochs may report a current-memory trend, but not a
  time-to-limit estimate.

## ADR-022: Report numeric rates without v0.7 risk classification

- **Status:** Approved
- **Decision:** v0.7 reports numeric current-memory rates and evidence quality without stable,
  rising, falling, anomaly, or risk labels.
- **Alternatives considered:** Direction labels with uncalibrated thresholds; immediate risk scores.
- **Reason:** Labels and risk scores need empirical calibration and backtesting before they can make
  reliable claims.
- **Consequence:** v0.8 evaluation may introduce calibrated classifications with documented evidence
  and limitations.

## ADR-023: Retain full container identity in JSON trend results

- **Status:** Superseded by ADR-037
- **Decision:** JSON trend results include the full Docker container ID and cgroup path. Terminal
  tables abbreviate container IDs only for readability.
- **Alternatives considered:** Redact identifiers everywhere; add an identifier-redaction option in
  v0.7.
- **Reason:** Local evidence already contains these identifiers, and full values are necessary for
  auditability and reliable joins by automation or a future dashboard.
- **Consequence:** A future sharing/export feature may add explicit redaction as a separately
  approved public contract.

## ADR-024: Emit one versioned JSON trend report document

- **Status:** Superseded by ADR-037
- **Decision:** JSON output is one versioned report document containing manifest metadata, a global
  summary, and a bounded array of epoch results.
- **Alternatives considered:** JSON Lines; an unversioned array of epoch objects.
- **Reason:** One document keeps report metadata and its results atomically connected and is well
  suited to the bounded v0.7 analysis size.
- **Consequence:** The JSON schema must define a `result_version` and be documented and tested as a
  public output contract.

## ADR-025: Treat valid ineligible reports as successful CLI execution

- **Status:** Approved
- **Decision:** `trend manifest` returns `0` for every valid report, including one with no eligible
  epochs; it returns `1` for input or runtime failures and `2` for usage failures.
- **Alternatives considered:** Return non-zero when no rate is available; assign one exit code per
  result condition.
- **Reason:** Evidence ineligibility is a valid, useful diagnostic outcome rather than a command
  failure.
- **Consequence:** Scripts can distinguish successful analysis from operational failure without
  treating sparse or insufficient evidence as an error.

## ADR-026: Close an epoch for every `memory.max` change

- **Status:** Approved
- **Decision:** Every semantic `memory.max` change closes an observation epoch, including finite-to-
finite, finite-to-unlimited, and unlimited-to-finite changes.
- **Alternatives considered:** Ignore limit changes; use an arbitrary percentage threshold.
- **Reason:** A changed hard memory budget changes the operational meaning of current memory and any
  limit-crossing estimate.
- **Consequence:** The same container may appear in multiple epochs within one report, each with its
  own limit context.

## ADR-027: Handle current-memory and limit availability separately

- **Status:** Approved
- **Decision:** An unavailable `memory.current` closes an epoch. An unavailable or unlimited
  `memory.max` preserves the current-memory trend but prevents a time-to-limit estimate.
- **Alternatives considered:** Close an epoch for every unavailable metric; skip unavailable current
  memory samples and join the remaining points.
- **Reason:** The usage rate cannot cross an unknown current-memory point, while a valid usage trend
  remains informative even without a usable hard-limit context.
- **Consequence:** Results expose the unavailable limit reason and never silently omit a metric.

## ADR-028: Reject overlapping sessions for one strict identity

- **Status:** Approved
- **Decision:** A manifest is rejected when selected evidence sessions overlap for the same full
  Docker container ID and cgroup path.
- **Alternatives considered:** Deduplicate equal timestamps; merge all concurrent samples.
- **Reason:** Concurrent evidence can contain duplicate or conflicting measurements, and selecting an
  authoritative point without an explicit rule would compromise the trend.
- **Consequence:** Users select a non-overlapping artifact set. MemOptima reports the conflicting
  sessions rather than producing an ambiguous trend.

## ADR-029: Represent absent containers as explicit observation gaps

- **Status:** Approved
- **Decision:** An absent container in a selected session is an explicit observation gap. If the same
  strict identity reappears later, it remains in the epoch and the gap contributes to sparse quality.
- **Alternatives considered:** End an epoch at every absence; silently ignore absences.
- **Reason:** A partial scan or a temporary absence is not necessarily a new workload, but it must
  never appear as uninterrupted observation.
- **Consequence:** Sparse epochs cannot produce time-to-limit estimates, and output exposes their
  evidence quality.

## ADR-030: Separate safe manifest loading from pure manifest parsing

- **Status:** Approved
- **Decision:** A filesystem adapter safely loads one manifest file, then a pure parser and validator
  converts its JSON text and source directory into a typed `TrendManifest` model.
- **Alternatives considered:** One combined file-reading parser; CLI-owned manifest parsing.
- **Reason:** Separating I/O from validation keeps the parser independently testable and prevents CLI
  code from becoming the owner of domain input rules.
- **Consequence:** The loader and parser have distinct error contracts, while future trend services
  consume the typed model without reparsing JSON.

## ADR-031: Preserve physical line-termination metadata for bounded manifest loading

- **Status:** Approved
- **Decision:** `EvidenceInputReader` exposes whether its most recently returned physical line ended
  with a newline byte. The trend manifest loader uses this metadata to enforce its aggregate byte
  limit while reading the already-opened descriptor.
- **Alternatives considered:** Count every returned line as newline-terminated; depend only on a
  pathname `file_size` check; replace the existing reader API with a new line-result type.
- **Reason:** Exact delimiter information prevents a file that grows during reading from bypassing
  the bounded loader while retaining valid manifests that occupy exactly the configured limit with
  no final newline.
- **Consequence:** Existing `next_line()` callers remain source-compatible. Consumers that need
  physical-byte accounting can use the additional read-only metadata.

## ADR-032: Stream artifacts through a bounded session catalog

- **Status:** Approved
- **Decision:** The trend session catalog retains ordered session metadata and one first/last
  observation window per strict container identity. It preserves incomplete-artifact status but does
  not retain every sample. A later analyzer rereads the validated, ordered artifacts through the
  streaming input path.
- **Alternatives considered:** Retain every sample in memory; create a temporary local spool file.
- **Reason:** The catalog must sort artifacts and reject ambiguous overlap without making memory
  consumption proportional to all JSONL input. Incomplete artifacts can still provide observed data
  when their status remains explicit.
- **Consequence:** The catalog is a reusable public `trends` interface. It limits each artifact to
  4,096 distinct container observation windows and leaves numeric trend calculation to the next
  streaming layer.

## ADR-033: Add an internal typed streaming evidence projection for trends

- **Status:** Approved
- **Decision:** v0.7 trends consume a minimal typed projection of validated schema-v1 evidence through
  explicit start, sample, and end callbacks. It includes parsed timestamps, normalized full Docker
  IDs, cgroup paths, `memory.current`, and the finite, unlimited, or unavailable state of
  `memory.max`. The adapter remains internal and is layered over the existing validator.
- **Alternatives considered:** Let every trend consumer parse canonical JSON; replace all existing
  evidence consumers in one migration; construct a separate trend validator.
- **Reason:** A typed projection removes JSON knowledge from trends while preserving bounded streaming
  behavior and avoiding a broad regression risk for stable v0.6 consumers.
- **Consequence:** The catalog is migrated first. Forecast and inspection may migrate separately after
  equivalent behavior is tested. PSI, swap, cache, assessment, and diagnostic details remain outside
  the v0.7 projection.

## ADR-034: Define a session-aware streaming trend analyzer

- **Status:** Approved
- **Decision:** v0.7 trend analysis is split into a pure, session-aware domain analyzer and an
  internal replay adapter. The adapter rereads cataloged evidence artifacts through the typed
  projection and supplies explicit session, sample, observation, and end events. The analyzer has
  no filesystem, JSON, Docker, or CLI dependency.
- **Observation model:** Session and sample events provide capture time and declared sampling
  interval. Every container observation carries a full Docker container ID, cgroup path, optional
  `memory.current`, and an explicit `memory.max` state: finite, unlimited, or unavailable. Missing
  current memory closes an epoch; unlimited or unavailable maximum preserves a usage trend but
  prevents a time-to-limit estimate.
- **Result model:** Each epoch has explicit evidence quality, rate status, and time-to-limit status.
  A rate is available only with at least three observations spanning at least ten seconds. It is
  calculated with ordinary least-squares linear regression across all eligible observations, using
  time offsets from the first observation. A time-to-limit estimate uses the latest observed current
  memory and that regression rate, is expressed as whole milliseconds rounded down, and additionally
  requires continuous evidence, a stable finite maximum, and positive growth. Reaching or exceeding
  the maximum is reported explicitly, not as a future estimate. Available durations use unsigned
  64-bit milliseconds; a non-finite or out-of-range duration reports an explicit status instead.
- **Continuity:** At each sample boundary, a container absent from that sample is an observation
  gap. A gap exceeding twice the larger adjacent declared interval makes the epoch sparse. A
  semantic maximum change closes an epoch. A non-increasing timestamp closes an epoch with an
  explicit continuity-break reason; input order is never sorted or silently repaired.
  When current memory is available, the observation with the non-increasing timestamp starts the
  next epoch, even if its maximum has also changed. An incomplete session marks every epoch to
  which it contributed an observation with available current memory, including epochs closed
  before the end of the session.
- **Status precedence:** A time-to-limit result reports one primary status in this order:
  already-at-or-above maximum, unlimited maximum, unavailable maximum, sparse evidence, unavailable
  rate, non-positive growth, duration out of range, then available. Quality and maximum context
  remain separate result fields, so lower-priority facts are not hidden.
- **Public API:** The public `trends` contract exposes plain aggregate input and result structures,
  strong C++ time and integer types, and one stateful `TrendAnalyzer`. It accepts ordered session,
  sample, observation, and session-end events through `std::expected`-returning methods, then
  returns one bounded and deterministically sorted `TrendAnalysis` from `finish()`. Event-order,
  duplicate-observation, noncanonical-metric, and resource-limit failures are errors; valid but
  ineligible evidence remains a result with explicit status fields. An analyzer instance is single
  use after a successful `finish()`. The analyzer accepts up to 4,096 distinct strict identities
  per sample, including observations with unavailable current memory. At capacity, an otherwise valid
  observation returns `duplicate_observation` for an existing identity and `resource_limit_exceeded`
  for a new identity. This bounds the state required for duplicate detection and sample membership.
  The analyzer accepts up to 4,096 active epochs and 4,096 total epochs, counting both active and
  closed epochs toward the total. `observe()` rejects an observation that would exceed either
  bound before changing sample membership or epoch state.
- **Alternatives considered:** A batch analyzer that owns file I/O; a flat observation-only API;
  endpoint-only rates; optional values without status reasons; sorting non-monotonic evidence.
  For sample membership, relying on the replay adapter alone would leave direct API calls unbounded.
- **Reason:** The design retains bounded streaming behavior while making analytical policy pure,
  deterministic, testable, explainable, and reusable by the future CLI, JSON report, and dashboard.
- **Consequence:** The next implementation introduces the specified public trend-result contract and
  pure analyzer tests before adding the `trend manifest` application use case or CLI presentation.

## ADR-035: Add an atomic validated trend replay service

- **Status:** Approved
- **Decision:** The public `trends` module adds one application service in
  `trend_analysis_service.hpp`:

  ```cpp
  [[nodiscard]] std::expected<TrendAnalysis, TrendAnalysisError>
  analyze_trend_manifest(const TrendManifest &manifest);
  ```

  The service first creates a bounded `TrendEvidenceCatalog`, then replays its ordered artifact
  paths through the internal typed evidence reader into one `TrendAnalyzer`. It returns either one
  complete `TrendAnalysis` or one `TrendAnalysisError`; it never exposes a partial result.
- **Replay contract:** For each cataloged artifact, replay maps the typed session start to
  `begin_session`, every typed sample to `begin_sample`, zero or more `observe` calls, and
  `end_sample`, then invokes `end_session` only after the artifact traversal succeeds. The replay
  traversal result supplies the `incomplete` value to `end_session`, so a valid artifact without an
  end record remains analytically explicit rather than being rejected or silently treated as
  complete.
- **Replay validation:** After an artifact is streamed and before its analyzer session is closed or
  any result is returned, replay compares its catalog-relevant metadata with the corresponding
  first-pass catalog entry: session start time, declared sampling interval, incomplete status, and
  the minimum/maximum captured timestamp windows for every strict container identity. Any difference
  is `evidence_replay_failed`. The service does not add a content hash in v0.7; the comparison
  protects the ordering and overlap facts established by cataloging while leaving valid replayed
  measurements as the analyzer input.
- **Error contract:** `TrendAnalysisError` has exactly three categories: `catalog_failed`,
  `evidence_replay_failed`, and `analyzer_failed`. It carries optional artifact context and an
  English diagnostic detail. Catalog errors are mapped to `catalog_failed`; typed-reader failures
  and catalog/replay mismatches are mapped to `evidence_replay_failed`; rejected analyzer events or
  finalization are mapped to `analyzer_failed`.
- **Alternatives considered:** Put cataloging and replay directly in the CLI; expose the internal
  typed reader publicly; retain all evidence samples between cataloging and analysis; accept replay
  metadata changes; add a whole-artifact cryptographic hash in v0.7.
- **Reason:** A small application boundary lets the future CLI, JSON report, dashboard, and API
  share one validated orchestration path without giving them access to internal evidence parsing.
  Revalidating catalog-defining metadata makes the two-pass streaming design auditable without
  making memory proportional to all selected evidence.
- **Consequence:** The later CLI calls this service and remains a presentation layer. Tests cover
  successful multi-artifact replay, each error category, incomplete artifacts, and detection of a
  catalog/replay metadata mismatch. v0.7 still does not claim an OOM prediction, a confirmed memory
  leak, or a safe automatic action.

## ADR-036: Name catalog observation windows by temporal bounds

- **Status:** Approved
- **Decision:** In the next C++ implementation change, replace the public
  `TrendContainerObservationWindow` members `first_captured_at_utc` and `last_captured_at_utc` with
  `minimum_captured_at_utc` and `maximum_captured_at_utc`. Until that change is merged, the existing
  member names remain the current API.
- **Reason:** The catalog calculates the minimum and maximum capture timestamps across every sample
  for one strict identity. The previous names could be read as first and last timestamps in artifact
  input order, which is a different contract and would weaken the meaning of overlap and replay
  validation.
- **Alternatives considered:** Retain the old names and document their nonliteral meaning; store
  both input-order and temporal-bound timestamps.
- **Consequence:** The next C++ change makes the public v0.7 API semantically precise before
  release. Catalog implementation, tests, and later replay comparison will use the renamed members.
  The change preserves runtime behavior but requires source updates for callers of this pre-release
  C++ API.

## ADR-037: Provide human-first trend analysis table output in v0.7

- **Status:** Approved
- **Supersession:** This decision supersedes ADR-010, ADR-016, ADR-023, and ADR-024. It also
  supersedes only the command- and output-format portions of ADR-017, ADR-019, and ADR-025; their
  per-epoch, evidence-quality, and successful-ineligible-result policies remain approved.
- **Decision:** v0.7 exposes validated trend analysis through the command
  `memoptima trends analyze <MANIFEST>`. The command calls the public
  `analyze_trend_manifest` application service and writes one stable, human-readable table to
  standard output. The table has one row per trend epoch and semantically exposes a Docker
  identifier, cgroup path, epoch index, observation count, evidence quality, growth rate when
  available, time-to-limit when available, and an explicit status. It does not present a trend
  result as an OOM prediction, a confirmed memory leak, or a safe automatic action.
- **Identifier presentation:** The table displays the complete cgroup path and a Docker ID prefix
  of at least twelve characters. The formatter extends prefixes as needed to distinguish Docker
  IDs in one result. A cgroup path distinguishes strict identities that share a Docker ID. Epoch
  indices are local to a strict identity rather than globally unique. The table remains an
  operational summary rather than a full identity-inspection view.
- **Table layout:** The table columns, in order, are `CONTAINER ID`, `CGROUP PATH`, `EPOCH`,
  `SAMPLES`, `WINDOW`, `QUALITY`, `RATE`, `TIME TO LIMIT`, and `STATUS`. Container identity,
  quality, and status columns are left-aligned. Epoch, sample, duration, rate, and time-to-limit
  columns are right-aligned. Each column width is at least the width of its header and otherwise
  expands to the longest value in the result. Columns are separated by exactly two spaces; `STATUS`
  is the final column and has no trailing padding before the newline. The formatter preserves the
  deterministic epoch order supplied by `TrendAnalysis`.
- **Availability presentation:** A valid analysis with ineligible epochs is successful. Unavailable
  rate or time-to-limit values are represented by explicit status text and a non-numeric table
  value, not by a fabricated zero or an omitted row. Evidence quality and result availability stay
  distinct so that sparse or incomplete evidence remains visible.
- **Quality presentation:** `QUALITY` is one of `continuous`, `sparse`,
  `continuous (incomplete)`, or `sparse (incomplete)`. Incomplete evidence is therefore visible
  without changing the independently reported time-to-limit status.
- **Rate presentation:** An available rate uses dynamic IEC units, two decimal places, and an
  explicit sign, for example `+1.25 MiB/s` or `-512.00 KiB/s`. The `RATE` column is right-aligned.
  A value that rounds to zero in its displayed unit is normalized to a positive sign, so output
  never presents `-0.00 B/s`. An unavailable rate is `N/A`.
- **Status presentation:** `STATUS` is `available`, `insufficient observations`,
  `insufficient observation window`, `sparse evidence`, `non-positive growth`,
  `unlimited memory.max`, `unavailable memory.max`, `at or above memory.max`, or
  `estimate out of range`. The first two unavailable statuses derive from rate eligibility; the
  remaining statuses derive from time-to-limit eligibility. An unavailable `TIME TO LIMIT` is
  `N/A`.
- **Empty analysis presentation:** A successful analysis with no container observation epochs
  writes `No container observation epochs found in manifest.` followed by a newline, without a
  table header, and returns `0`.
- **Operational duration presentation:** The table uses a shared CLI formatter that accepts a
  millisecond duration. Durations under one second are `< 1s`; durations under one minute are whole
  seconds; durations under one hour are `Xm YYs`; durations under one day are `Xh YYm`; and longer
  durations are `Xd YYh`. Every displayed unit is rounded down, never to the nearest unit or up.
  The formatter does not change the existing microsecond-oriented duration formatter used by other
  CLI output.
- **Exit contract:** The command returns `0` after a successful analysis, including analyses that
  contain unavailable results; `1` for manifest loading or parsing, `catalog_failed`,
  `evidence_replay_failed`, `analyzer_failed`, or failure to write the table to standard output;
  and `2` for invalid command syntax or arguments. Successful tables are written only to standard
  output. Diagnostics and usage errors are written only to standard error.
- **Deferred machine contract:** v0.7 does not provide a JSON flag, a report file, or a
  machine-readable trend-output schema. A versioned JSON report is reconsidered in v0.8 after PSI,
  swap, cache, and intelligence-layer requirements define the complete reporting model.
- **Alternatives considered:** Deliver a table and a versioned JSON schema together; expose JSON
  only; keep the service internal until v0.8; show full Docker IDs in every table row; silently
  omit ineligible epochs.
- **Reason:** A table completes the user-facing evidence-to-analysis workflow for incident response
  while avoiding a premature machine-readable compatibility promise. Explicit statuses protect
  operators from overinterpreting limited evidence, and adaptive prefixes preserve container
  identity without routinely making the table impractically wide.
- **Consequence:** The CLI remains a presentation layer over the public trends service. The
  implementation requires formatter and command integration tests for successful analyses,
  unavailable results, same-Docker-ID distinct-cgroup identities, diagnostics, and all three
  exit-code classes, including every public trend-service error category, failed table output,
  empty analysis output, and every operational duration boundary and rounding transition. Any later
  JSON schema requires a separate approved decision. Formatter tests cover every quality and status
  label, dynamic rate units, negative-zero normalization, unavailable values, column alignment,
  dynamic column widths, and absent trailing padding.

## ADR-038: Release the approved cross-session trend capability as version 0.7.0

- **Status:** Approved
- **Decision:** MemOptima's next public release is version `0.7.0`. The CMake project version remains
  the single source of truth and therefore changes the generated application version used by
  `memoptima --version` and by newly written evidence artifacts. The evidence schema version remains
  `1`.
- **Alternatives considered:** Keep the public version at `0.6.0` until v0.8; publish the trends
  capability as a patch release; introduce a separate version source for evidence artifacts.
- **Reason:** v0.7 adds the owner-approved cross-session trend module and public
  `memoptima trends analyze <MANIFEST>` command. A minor version communicates this additive,
  backward-compatible product capability while preserving one auditable build-version source.
- **Consequence:** Release preparation updates the CMake version to `0.7.0`, extends the release
  verification to cover the trends command under Valgrind, and records the actual validation results
  in a v0.7.0 verification document. Existing schema-v1 artifacts remain valid; new sampling
  artifacts report application version `0.7.0` without changing their schema.

## ADR-039: Preserve evidence availability, bounded consumption, and path containment

- **Status:** Approved
- **Decision:** A forecast result may expose a numeric growth rate and estimated time to
  `memory.max` only when its final status is `available`. If any later observation makes the
  container unavailable, the result retains the explicit unavailable status and clears both numeric
  values. Evidence readers accept at most 512 MiB in total, matching the existing maximum size of
  an artifact MemOptima can produce. Trend artifact paths remain relative to and contained within
  the manifest directory, but are opened descriptor-relative one path component at a time with
  `openat` and `O_NOFOLLOW`; containment is therefore enforced at open time rather than only by a
  prior pathname check.
- **Alternatives considered:** Retain calculated forecast values beside an unavailable final
  status; impose a smaller reader-only evidence limit; retain canonical-path validation followed by
  ordinary pathname opening.
- **Reason:** A stale estimate beside unavailable evidence can mislead an operator. Matching the
  reader limit to the output limit preserves round-trip compatibility while bounding resource use.
  Descriptor-relative traversal removes the race in which an intermediate directory changes after
  validation but before a selected artifact is opened.
- **Consequence:** The forecast table output never presents an estimate for an unavailable result.
  Validation tracks aggregate physical input bytes as well as individual record size. Trend input
  requires Linux descriptor-based traversal and receives regression tests for late metric loss,
  aggregate input limits, and intermediate-path replacement.

## ADR-040: Reject NUL bytes in trend artifact paths

- **Status:** Approved
- **Decision:** Manifest version 1 rejects an artifact `path` containing a NUL byte. The internal
  descriptor-relative evidence opener independently rejects a NUL byte in every path component
  before calling a filesystem API.
- **Alternatives considered:** Rely only on lexical manifest validation; rely only on the internal
  opener; permit NUL bytes and rely on platform pathname behavior.
- **Reason:** JSON strings can represent a NUL byte, while POSIX path APIs interpret NUL as a
  terminator. Rejecting it at the public input boundary and again immediately before `openat`
  ensures validation and filesystem traversal operate on the same path representation.
- **Consequence:** A manifest with `\u0000` in an artifact path is invalid. Regression tests cover
  both manifest parsing and the internal descriptor-relative boundary. The evidence schema and
  valid manifest paths remain unchanged.

## ADR-041: Require a non-symbolic manifest directory path

- **Status:** Approved
- **Decision:** `load_trend_manifest` rejects a manifest path whose original directory component
  sequence contains a symbolic link, before lexical normalization can hide it. `parse_trend_manifest`
  remains a pure parser and retains no filesystem resolution responsibility.
- **Alternatives considered:** Resolve the supplied directory in the parser; accept a symbolic
  directory path in the loader and resolve it before descriptor-relative artifact traversal.
- **Reason:** The parser is intentionally independent of filesystem state under ADR-030. Allowing
  the loader to resolve a symbolic directory while a directly parsed manifest later fails the
  descriptor-relative traversal produces inconsistent behavior for one manifest location.
- **Consequence:** Operators must invoke the trend command through a physical, non-symbolic path to
  the manifest directory. Ordinary relative paths such as `manifest.json` and `./manifest.json`
  remain valid. Artifact traversal continues to reject symbolic links at every component, and the
  parser remains independently testable without filesystem I/O.

## ADR-042: Evaluate forecast accuracy with bounded temporal holdouts in v0.8

- **Status:** Approved
- **Decision:** v0.8 adds the public command
  `memoptima forecast evaluate <MANIFEST>`. It evaluates forecasted `memory.current` against a
  later, actually observed `memory.current` for each strict container identity
  (`container_id`, `cgroup_path`). The evaluator uses a temporal holdout: it computes a forecast
  from observations available before a selected later observation, never from future observations.
  Existing `memoptima forecast evidence <FILE>` behavior remains unchanged.
- **Metrics:** The human-readable report provides eligible and evaluated forecast counts, coverage
  (`evaluated / eligible`), mean absolute error (MAE), mean signed error (bias), and explicit
  status counts. It also provides one row per evaluated or ineligible container epoch. MAE and bias
  include only evaluated rows. v0.8 does not make percentage error a primary metric.
- **Eligibility:** An eligible row has a continuous historical usage epoch, available
  `memory.current` values, strictly increasing timestamps, and the existing minimum observation and
  duration requirements. A row is evaluated only when that eligible history has a later observation
  of the same strict identity with available `memory.current`. Therefore coverage can honestly
  report an eligible forecast that lacks a usable holdout. `memory.max` is not an eligibility
  requirement because the evaluation measures predicted memory usage, not time to a memory limit.
  A non-applicable, unavailable, or changing `memory.max` must remain visible where relevant but
  must not discard an otherwise valid usage evaluation. Non-positive growth is valid for usage
  evaluation and is not an unavailable condition.
- **Holdout selection:** Each usage epoch produces at most one row. The evaluator selects the first
  instant at which the historical observations satisfy eligibility, then uses the immediately next
  observation as its holdout. If that next observation is unavailable or absent because the epoch
  ends, the row remains eligible with `holdout_observation_unavailable`. The evaluator does not
  select the final available observation as a holdout by default, because that would make coverage
  trivially approach 100 percent and hide trace-ending limitations.
- **Usage continuity:** Evaluation uses its own usage-continuity policy rather than the trend
  module's time-to-limit continuity policy. A strict identity, available `memory.current`, and
  strictly increasing timestamps are required. A `memory.max` change, unavailable maximum, or
  unlimited maximum does not break usage continuity. A required missing container observation or
  unavailable historical `memory.current` does break it.
- **Result contract:** Each row exposes the strict identity, local usage-epoch index, historical
  start and end timestamps, optional holdout timestamp, historical observation count and window,
  stable status, and—only when evaluated—predicted and actual current-memory values plus signed and
  absolute error. The report summary exposes row, eligible, evaluated, and incomplete-exclusion
  counts; per-status counts; optional coverage; optional MAE; optional bias; and the rows. Numeric
  calculations retain `double` precision; presentation performs rounding. A negative linear
  projection is clamped to `0 B` before it becomes a predicted value or contributes to an error
  metric.
- **Row statuses:** The stable row statuses are `evaluated`, `excluded_incomplete_session`,
  `insufficient_observations`, `insufficient_observation_window`,
  `container_not_observed_continuously`, `historical_current_memory_unavailable`,
  `non_increasing_timestamps`, `holdout_observation_unavailable`, and
  `numeric_result_unavailable`. Only `evaluated` rows contribute to MAE and bias. Eligible rows
  exclude incomplete-session rows, even when their history would otherwise be sufficient.
- **Incomplete evidence:** A row affected by an incomplete sampling session remains visible with an
  explicit status, but is excluded from eligible and evaluated counts, coverage, MAE, and bias.
- **Resource and output contract:** Evaluation reuses the validated manifest and evidence-reader
  boundaries: no more than 128 artifacts, aggregate bounded evidence input, safe descriptor-relative
  opening, and at most 4,096 result rows. It streams evidence and retains only bounded per-identity
  evaluation state. A resource-limit failure returns no partial report. v0.8 provides a stable
  human-readable table only; a machine-readable, versioned report is deferred to a later approved
  decision.
- **Application service:** The public
  `evaluate_forecast_manifest(const trends::TrendManifest &)` service first creates the existing
  validated trend evidence catalog, then replays every cataloged artifact through the public
  root-relative evidence stream into one evaluator. It compares replay metadata against the catalog
  before closing each evaluator session and returns either one complete report or one categorized
  error: `catalog_failed`, `evidence_replay_failed`, or `analyzer_failed`.
- **Human-readable output:** v0.8 presents an evaluation through a summary followed by one
  dynamically sized table. The summary shows eligible forecasts, evaluated forecasts, coverage,
  MAE, bias, incomplete exclusions, and nonzero status counts. The table columns are `CONTAINER ID`,
  `CGROUP PATH`, `EPOCH`, `HISTORY`, `WINDOW`, `FORECAST AT`, `HOLDOUT AT`, `PREDICTED`, `ACTUAL`,
  `ERROR`, and `STATUS`. Full cgroup paths and canonical UTC timestamps preserve local auditability.
  Numeric byte values use IEC units and two decimal places; row error is signed
  (`predicted - actual`), while MAE remains an absolute summary metric. Values unavailable for a row
  are `N/A`. Numeric and duration columns are right-aligned, identity, timestamp, and status columns
  are left-aligned, and the final column has no trailing padding. A valid evaluation with no rows
  writes `No forecast evaluation epochs found in manifest.` and succeeds without a table or summary.
- **CLI and exit contract:** The public command is `memoptima forecast evaluate <MANIFEST>`.
  Existing `memoptima forecast evidence <FILE>` behavior remains unchanged. Successful evaluation,
  including an empty valid result, returns `0` and writes only to standard output. Manifest loading,
  cataloging, replay, analyzer, formatting, or standard-output write failures return `1` and write
  diagnostics only to standard error. Invalid command syntax returns `2`, with usage output on
  standard error.
- **Non-goals:** v0.8 does not evaluate time-to-limit accuracy, predict OOM, diagnose a confirmed
  memory leak, classify an anomaly as a defect, or make an automatic change.
- **Alternatives considered:** Evaluate every possible forecast point; include future observations
  in fitting; select the final eligible forecast in an epoch; require a stable finite `memory.max`;
  require positive growth; retain negative linear projections; include incomplete sessions in
  aggregate metrics; publish JSON output together with the first evaluation command; evaluate
  time-to-limit accuracy in the same release.
- **Reason:** A bounded temporal holdout measures the forecast under the same information boundary
  that applies in operation. It supplies auditable accuracy evidence before higher-level advisory
  features rely on forecast results, while preserving the existing evidence and CLI contracts.
- **Consequence:** The first v0.8 implementation introduces a public evaluation-result contract,
  an atomic manifest application service, deterministic fixtures for available and unavailable
  cases, a human-first CLI formatter, and documentation that distinguishes forecast evaluation from
  OOM prediction and leak diagnosis.

## ADR-043: Expose validated evidence streaming as a lower-level module

- **Status:** Approved
- **Decision:** The public `memoptima::evidence` module exposes a typed, read-only schema-v1
  artifact stream through `ArtifactSink` and `traverse_artifact_beneath(root, relative_path, sink)`.
  The stream projects validated session-start, sample, and session-end records; container
  observations retain a full Docker ID, cgroup path, optional `memory.current`, and memory-maximum
  context. Session-start and sample events retain both typed timestamps and their canonical schema-v1
  UTC strings, so higher layers can validate a replay without depending on private timestamp
  formatting. The module exposes neutral evidence outcome and traversal-error types rather than
  public dependencies on sampling implementation types.
- **Safety boundary:** The public traversal intentionally accepts only a trusted root and a relative
  artifact path. It retains existing descriptor-relative traversal, `O_NOFOLLOW`, schema and
  lifecycle validation, and reader resource limits. It does not expose a public arbitrary-path
  traversal operation.
- **Dependency direction:** `trends` and `forecast_evaluation` consume this public lower-level
  interface. They must not include private typed-reader, JSON, or filesystem implementation
  headers. The existing private reader remains the implementation behind the public projection.
- **Alternatives considered:** Let forecast evaluation include the private typed reader; duplicate
  evidence parsing for evaluation; expose an unrestricted arbitrary-path reader; make evidence
  depend publicly on sampling outcome types.
- **Reason:** One validated streaming contract avoids duplicating safety-sensitive evidence parsing
  while preserving the project's requirement that modules depend only on public lower-level
  interfaces.
- **Consequence:** The existing trend catalog and replay use the public evidence stream without a
  behavior change. Public-contract regression tests cover valid projection and existing trend tests
  continue to cover cataloging, replay, and symbolic-link protections.

## ADR-044: Release forecast evaluation as version 0.8.0

- **Status:** Approved
- **Decision:** MemOptima's next public release is version `0.8.0`. The CMake project version
  remains the single source of truth and therefore changes the generated application version used by
  `memoptima --version` and by newly written evidence artifacts. The evidence schema version remains
  `1` and existing schema-version-1 artifacts remain valid.
- **Alternatives considered:** Keep the public version at `0.7.0` until advisory intelligence is
  added; publish forecast evaluation as a patch release; introduce a separate version source for
  evidence artifacts.
- **Reason:** `memoptima forecast evaluate <MANIFEST>` is an additive, backward-compatible public
  capability with a documented report contract and new evidence-based accuracy metrics. A minor
  version communicates that product expansion while retaining one auditable build-version source.
- **Consequence:** Release preparation changes the CMake version to `0.8.0`, installs the v0.8 scope
  and forecast-evaluation report documentation, and verifies the exact executable and installed
  version through the existing version and installation tests. New sampling artifacts report
  application version `0.8.0` without changing their schema.

## ADR-045: Render artifact-derived terminal text safely and clarify stream finality

- **Status:** Approved
- **Decision:** v0.9 adds one internal CLI formatting function for artifact-derived table cells. It
  renders a literal backslash as `\\`, newline as `\n`, carriage return as `\r`, tab as `\t`, and
  every other ASCII control byte, including ESC and DEL, as uppercase `\xHH`. The transformation is
  display-only: original cgroup paths remain unchanged for parsing, identity comparison, and
  analysis. The rule applies only to `CGROUP PATH` cells emitted by `memoptima trends analyze` and
  `memoptima forecast evaluate`; container IDs, headers, and internal statuses are not artifact text
  requiring this transformation.
- **ArtifactSink contract:** `consume` callbacks receive validated but provisional events. A later
  validation or I/O failure can make traversal fail after callbacks have already run. The evidence
  module does not roll back callback effects, so durable or external consumers must stage effects
  until `traverse_artifact_beneath` returns successfully.
- **Alternatives considered:** Reject every artifact containing a control byte; escape every CLI
  table cell; expose a public generic escaping API; leave literal backslashes unescaped; promise
  transactional callbacks or buffer every artifact before delivery.
- **Reason:** Evidence fields are local input, not terminal commands. Visible escaping prevents an
  artifact-controlled path from changing terminal layout or issuing terminal control sequences while
  retaining auditability. A callback-finality contract prevents future consumers from incorrectly
  treating a partial traversal as a committed artifact.
- **Consequence:** Ordinary printable paths retain their existing output. The two affected
  formatters share one internal implementation and receive regression tests. The public evidence
  header and architecture documentation state the provisional-callback rule without adding a new
  runtime API or changing the evidence schema.

## ADR-046: Release terminal and stream hardening as version 0.9.0

- **Status:** Approved
- **Decision:** MemOptima's next public release is version `0.9.0`. The CMake project version
  remains the single source of truth for `memoptima --version` and for the application version
  written into newly sampled evidence artifacts. The evidence schema version remains `1`, and
  existing schema-version-1 artifacts remain valid.
- **Alternatives considered:** Keep the public version at `0.8.0` because v0.9 adds no monitoring
  feature; publish a patch release; add a separate version source for evidence artifacts.
- **Reason:** v0.9 changes two production contracts: terminal rendering of artifact-derived cgroup
  paths and the documented finality of public evidence-stream callbacks. A minor release makes
  those safety-relevant contracts visible while preserving one auditable build-version source.
- **Consequence:** Release preparation changes the CMake version to `0.9.0`, installs the v0.9
  scope document, and verifies the exact executable and installed version through the existing
  version and installation tests. Newly sampled artifacts report application version `0.9.0`
  without changing their schema.

## ADR-047: Harden untrusted evidence text at validation and terminal boundaries

- **Status:** Approved
- **Decision:** Version 0.9.1 will preserve evidence schema version 1 while treating
  artifact-derived text as untrusted at every terminal-rendering boundary. The schema validator will
  require every container ID to contain exactly 64 hexadecimal ASCII characters, matching the Docker
  container-ID parser, and every application version to contain from 1 through 64 printable ASCII
  bytes (0x20 through 0x7E). Values outside those contracts will be rejected as invalid
  schema-version-1 artifact content.
- **Display rule:** One internal CLI display function will render untrusted bytes safely in standard
  output and standard error. It will escape literal backslashes, C0 controls, DEL, C1 controls,
  bidi controls (U+061C, U+200E–U+200F, U+202A–U+202E, and U+2066–U+2069), zero-width controls
  (U+200B–U+200D, U+2060, and U+FEFF), and line or paragraph separators (U+2028 and U+2029).
  Invalid UTF-8 bytes and unlisted C0 controls will use uppercase \xHH notation; the listed
  decoded Unicode controls will use uppercase \u{...} notation. Newline, carriage return, tab,
  and a literal backslash retain the unambiguous \n, \r, \t, and \\ forms. Other valid,
  printable Unicode remains unchanged.
- **Coverage:** The display rule applies to artifact-derived text, manifest-derived paths,
  command-line paths repeated in output, filesystem or errno diagnostics that can contain such
  paths, and live cgroup-directory warnings. It covers all standard-output and standard-error
  paths used by inspect evidence, forecast evidence, trends analyze, and forecast evaluate.
- **Compatibility:** The validation tightening intentionally rejects malformed artifacts that were
  previously accepted. MemOptima's own writers already emit lowercase 64-character Docker IDs and
  version values compatible with the new grammar. The patch release is justified as a
  security-boundary correction; it does not change the evidence schema number.
- **Verification:** One end-to-end terminal-safety test will exercise every artifact-reading command
  with a poisoned artifact and manifest. It will prove that no listed raw control byte or decoded
  Unicode control reaches either standard output or standard error.
- **Alternatives considered:** Leave validation permissive and escape only individual table cells;
  reject every non-ASCII value; introduce a public trusted-text type immediately; preserve
  acceptance of invalid IDs; delay all validation changes to v0.10.
- **Reason:** v0.9.0 safely renders cgroup paths in two tables but does not cover artifact-derived
  IDs, application versions, paths, or diagnostics in other artifact-facing commands. Requiring
  valid identifiers at the shared schema boundary and rendering text safely at the shared
  presentation boundary prevents a future formatter from silently restoring terminal-control
  exposure.
- **Consequence:** v0.9.1 will have a deliberately broader output contract than v0.9.0 and requires
  exact regression tests. Duplicate JSON-key rejection is not part of this patch because it needs a
  separately designed parser policy; it is required planning work for v0.10 before Intelligence
  features.

## Pending decisions

| ID | Decision required before implementation |
|---|---|
