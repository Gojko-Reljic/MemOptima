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

## Pending decisions

| ID | Decision required before implementation |
|---|---|
