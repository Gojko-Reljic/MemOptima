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

- **Status:** Approved
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

- **Status:** Approved
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

- **Status:** Approved
- **Decision:** JSON trend results include the full Docker container ID and cgroup path. Terminal
  tables abbreviate container IDs only for readability.
- **Alternatives considered:** Redact identifiers everywhere; add an identifier-redaction option in
  v0.7.
- **Reason:** Local evidence already contains these identifiers, and full values are necessary for
  auditability and reliable joins by automation or a future dashboard.
- **Consequence:** A future sharing/export feature may add explicit redaction as a separately
  approved public contract.

## ADR-024: Emit one versioned JSON trend report document

- **Status:** Approved
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

## Pending decisions

| ID | Decision required before implementation |
|---|---|
