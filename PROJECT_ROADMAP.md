# MemOptima Product Roadmap

## Product vision

MemOptima is a Linux-first, C++23 Predictive Memory Intelligence platform for observing,
explaining, forecasting, and eventually helping operators optimize memory behavior in Linux hosts
and Docker containers.

The name **MemOptima 2.0** describes the long-term product vision. It is not the current semantic
version of the executable.

## Product principles

- Preserve correctness, explainability, and memory safety before adding capability.
- Keep the core application in C++23 and organize it as independently testable modules.
- Make claims only when evidence supports them.
- Keep observation and advice local and read-only until a separately approved action design exists.
- Preserve compatible CLI behavior and evidence schema contracts.

## Current baseline: v0.8

v0.8 collects Linux and Docker cgroup v2 memory evidence, validates bounded local schema-v1 JSON
Lines artifacts, produces conservative memory-limit forecasts, analyzes `memory.current` trends
across explicitly selected local evidence sessions, and evaluates the first eligible forecast against
its immediately following holdout observation. It does not predict OOM events, confirm memory leaks,
or modify the host or containers.

## Completed: v0.7 historical trend foundation

### Approved direction

v0.7 adds a local, read-only cross-session trend capability. It analyzes a manifest that explicitly
names a selected set of evidence artifacts rather than introducing a daemon, a database service, or
automatic actions.

The manifest is one versioned JSON document with a closed schema and an explicit artifact list.
Artifact paths are relative to the manifest directory and cannot escape its directory tree.
MemOptima orders the selected artifacts by validated session start time and rejects duplicates.
It fails closed when any artifact is unavailable, unsafe, malformed, or lifecycle-invalid.
Manifest version 1 is closed and contains only `manifest_version`, `artifacts`, and artifact `path`.
The public command is `memoptima trends analyze <MANIFEST>`, with one human-readable table on
standard output. A machine-readable, versioned JSON trend report is deferred to a later release.
Each result represents one container observation epoch; global summaries do not merge separate epochs.
Version 0.7 trends `memory.current`; `memory.max` provides only limit context and eligibility.
Time gaps remain visible quality signals: they mark a trend as sparse and prohibit a time-to-limit
estimate, but do not by themselves end a strict-identity epoch.
An interval is sparse only when it exceeds twice the larger adjacent declared sampling interval.
`memory.current` rate requires at least three observations across ten seconds; a time-to-limit estimate
also requires continuous quality and a stable finite `memory.max`.
Version 0.7 reports numeric rates and evidence quality, not uncalibrated direction labels or risk scores.
Terminal tables abbreviate Docker IDs while the evidence artifacts retain full Docker and cgroup
identities for local auditability.
The trend command returns success for every valid report, including reports with no eligible epochs.
Every semantic `memory.max` change closes an observation epoch.
Unavailable `memory.current` closes an epoch; unavailable or unlimited `memory.max` preserves the
usage trend but prevents a time-to-limit estimate.
Overlapping evidence sessions for the same strict container identity are rejected as ambiguous input.
An absent container becomes an explicit observation gap, not a silently continuous period or a new
identity.

### Delivered outcomes

- Typed internal evidence traversal that preserves schema-v1 compatibility.
- Explicit domain, adapter, application, and presentation boundaries.
- Cross-session continuity and comparability diagnostics.
- Container observation epochs based on an unchanged full Docker ID and cgroup path, preventing
  unsafe conclusions across recreation, resets, or gaps.
- Trend reports for memory usage and forecast eligibility across compatible sessions.
- Stable result codes for invalid artifacts, valid-but-ineligible data, and available trends.
- Streaming analysis limited to 128 manifest artifacts and 4,096 active container observation epochs.
- A human-readable table for local operational analysis; a versioned JSON result format is deferred
  to a later release.
- Deterministic fixtures covering gaps, interrupted sessions, limit changes, unavailable metrics, and
  container identity changes.

### Explicit non-goals

- No daemon, scheduler, remote service, database, dashboard, alert delivery, or automatic tuning.
- No assertion that a persistent growth pattern confirms a memory leak.
- No OOM prediction or automatic container/cgroup action.

## v0.8: predictive evaluation and advisory intelligence

### Planned outcomes

- Replay and temporal-holdout backtesting of `memory.current` forecasts using retained evidence
  traces, with the first eligible forecast attempt in each usage epoch.
- Documented eligible and evaluated counts, coverage, MAE, bias, stable row statuses, and explicit
  incomplete-evidence exclusions.
- A human-readable `memoptima forecast evaluate <MANIFEST>` report with per-epoch outcomes and
  aggregate metrics.
- Explainable advisory reports connecting assessments, trends, and safe operator investigation steps.
- Conservative indicators for anomalous behavior and suspected persistent memory growth.
- Provenance for the evidence, rules, and model inputs behind every advisory result.

### Explicit non-goals

- No claim that an anomaly or persistent growth indicator proves a defect or memory leak.
- No automatic remediation, resource-limit changes, or rollback implementation.
- No v0.8 evaluation claim about time-to-limit accuracy or OOM prediction.
- No machine-readable evaluation-report contract until a separately approved versioned schema.

## v0.9: terminal rendering and streaming-contract hardening

### Planned outcomes

- A shared internal CLI formatter that renders control bytes in artifact-derived cgroup paths
  visibly and without terminal control effects.
- Identical safe rendering in `trends analyze` and `forecast evaluate`, while preserving the
  unmodified path for identity and analysis.
- An explicit public `ArtifactSink` lifecycle contract that callbacks are provisional until artifact
  traversal succeeds.
- Regression tests for control bytes, terminal escape sequences, normal printable paths, and
  traversal failure behavior.

### Explicit non-goals

- No evidence-schema change, artifact rejection rule, identity normalization, or change to ordinary
  printable CLI output.
- No generalized terminal-escaping migration outside the two evidence-derived table cells.
- No new monitoring, forecasting, intelligence, recommendation, or automatic-action capability.

## Beyond v0.8

Potential future work includes a bounded service mode, dashboard, alerts, external integrations, and
an explicitly approved least-privilege action executor. Each requires its own scope, security model,
and owner decision before implementation.

## Release gates

Every release must preserve existing behavior unless an owner-approved decision states otherwise and
must pass appropriate formatting, unit, integration, sanitizer, Release memory-safety, installation,
and authorized Docker integration checks.
