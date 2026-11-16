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

## Current baseline: v0.6

v0.6 collects Linux and Docker cgroup v2 memory evidence, validates local schema-v1 JSON Lines
artifacts, and produces a conservative estimate of a finite `memory.max` crossing from one
continuous evidence session. It does not predict OOM events, confirm memory leaks, or modify the
host or containers.

## v0.7: historical trend foundation

### Approved direction

v0.7 adds a local, read-only cross-session trend capability. It will analyze a manifest that
explicitly names a selected set of evidence artifacts rather than introduce a daemon, a database
service, or automatic actions.

### Planned outcomes

- Typed internal evidence traversal that preserves schema-v1 compatibility.
- Explicit domain, adapter, application, and presentation boundaries.
- Cross-session continuity and comparability diagnostics.
- Container observation epochs based on an unchanged full Docker ID and cgroup path, preventing
  unsafe conclusions across recreation, resets, or gaps.
- Trend reports for memory usage and forecast eligibility across compatible sessions.
- Stable result codes for invalid artifacts, valid-but-ineligible data, and available trends.
- Streaming analysis limited to 128 manifest artifacts and 4,096 active container observation epochs.
- A human-readable table by default and a versioned JSON result format for integrations.
- Deterministic fixtures covering gaps, interrupted sessions, limit changes, unavailable metrics, and
  container identity changes.

### Explicit non-goals

- No daemon, scheduler, remote service, database, dashboard, alert delivery, or automatic tuning.
- No assertion that a persistent growth pattern confirms a memory leak.
- No OOM prediction or automatic container/cgroup action.

## v0.8: predictive evaluation and advisory intelligence

### Planned outcomes

- Replay and backtesting of forecast methods using retained evidence traces.
- Documented coverage, error, and failure measures relative to simple baselines.
- Explainable advisory reports connecting assessments, trends, and safe operator investigation steps.
- Conservative indicators for anomalous behavior and suspected persistent memory growth.
- Provenance for the evidence, rules, and model inputs behind every advisory result.

### Explicit non-goals

- No claim that an anomaly or persistent growth indicator proves a defect or memory leak.
- No automatic remediation, resource-limit changes, or rollback implementation.

## Beyond v0.8

Potential future work includes a bounded service mode, dashboard, alerts, external integrations, and
an explicitly approved least-privilege action executor. Each requires its own scope, security model,
and owner decision before implementation.

## Release gates

Every release must preserve existing behavior unless an owner-approved decision states otherwise and
must pass appropriate formatting, unit, integration, sanitizer, Release memory-safety, installation,
and authorized Docker integration checks.
