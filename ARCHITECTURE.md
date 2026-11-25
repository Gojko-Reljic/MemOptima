# MemOptima Architecture

## Authority and scope

This document is the canonical architecture reference for MemOptima. It describes the current v0.7
baseline and the approved direction toward v0.8. Product scope is defined in
[`PROJECT_ROADMAP.md`](PROJECT_ROADMAP.md); approved architectural choices are recorded in
[`DECISIONS.md`](DECISIONS.md).

## Current runtime flow

```text
Linux and Docker adapters
        ↓
typed snapshots and metric availability
        ↓
point-in-time analysis or bounded sampling
        ↓
schema-v1 JSON Lines evidence artifact
        ↓
safe validation and lifecycle checking
        ↓
inspection, forecast, or future trend use case
        ↓
CLI presentation
```

## Logical modules

| Module | Responsibility | Must not depend on |
|---|---|---|
| `linux/` | `/proc`, cgroup v2, and Docker discovery adapters | CLI formatting or forecast policy |
| snapshot models | Typed measurements and individual metric availability | Filesystem, JSON, or terminal APIs |
| `analysis/` | Pure point-in-time pressure and cgroup assessment | Docker discovery, file I/O, JSON, or CLI |
| `sampling/` | Bounded collection orchestration and evidence serialization | CLI formatting |
| `evidence/` | Bounded safe input opening, schema/lifecycle validation, and typed artifact streaming | Forecast or recommendation policy |
| `forecast/` | Pure eligibility and forecasting calculations | Filesystem, Docker, JSON, or CLI |
| `trends/` | Cross-session comparability, epochs, and trend calculations | Filesystem, JSON, or CLI |
| forecast evaluation | Bounded temporal holdout and aggregate accuracy metrics | CLI presentation or raw filesystem input |
| application use cases | Wire validated inputs, domain services, and stable result contracts | Terminal presentation details |
| `cli/` | Argument parsing, dependency wiring, and presentation | Domain implementation details |

The current build uses `memoptima_core`, `memoptima_cli_support`, and the executable. Logical
boundaries are enforced by these rules before any optional future split into additional CMake targets.

## Dependency direction

```text
adapters → domain models ← pure analysis / forecast / trends
                    ↑
           application use cases
                    ↓
               CLI presentation
```

Modules may use only public interfaces of lower-level modules. Internal parser details, JSON
representations, filesystem paths, and CLI formatting are not domain interfaces.

## Evidence contract

Schema-v1 JSON Lines artifacts remain a stable external contract. A valid artifact is not
automatically eligible for a forecast or a cross-session trend. Consumers must report the distinction
between invalid evidence, valid-but-ineligible evidence, and an available result.

v0.7 introduced typed internal start, sample, and end records behind the existing validators. This
preserves schema-v1 acceptance, safe input handling, and existing CLI behavior.

Evidence input is bounded by a 512 MiB aggregate physical-byte limit, including newline bytes. Trend
artifact paths are opened relative to the manifest directory through directory descriptors with
`O_NOFOLLOW` applied at every path component. This rejects symbolic links and prevents a later
filesystem replacement from escaping the manifest directory tree.

The public `evidence/` module is the only reusable typed streaming boundary for validated schema-v1
artifacts. It accepts a trusted root and a relative artifact path, retains descriptor-relative safe
opening, and projects typed session, sample, and terminal events. It does not expose raw JSON or an
arbitrary-path reader. Timestamp events retain their canonical schema-v1 UTC text beside their typed
value so replay validation does not require a private formatter. Higher-level consumers such as
trends and forecast evaluation use this public contract rather than private parser or filesystem
implementation headers.

Artifact sink callbacks are provisional: traversal can fail after it has delivered one or more
validated events. A successful return from `traverse_artifact_beneath` is the only completion signal
for the complete artifact. Consumers that perform durable writes or external actions must stage them
until that success result; the evidence module does not provide callback rollback.

## Terminal rendering safety

Evidence-derived cgroup paths are semantic identity values, but terminal rendering is a separate
boundary. The CLI preserves the original value for parsing and analysis, then renders ASCII control
bytes visibly in table cells. It uses `\\` for a literal backslash, `\n`, `\r`, and `\t` for those
named controls, and uppercase `\xHH` for every other ASCII control byte, including escape and DEL.

The v0.9 rule applies to `CGROUP PATH` cells in the trend and forecast-evaluation tables. It does
not reinterpret, validate, or normalize the original identity, and it leaves ordinary printable
paths unchanged. A shared internal CLI formatter owns the display-only transformation so both
tables cannot diverge.

## Cross-session trend policy

Cross-session analysis is local and opt-in. It accepts a manifest that explicitly names the evidence
artifacts to analyze and must model an observation epoch before comparing container data across
sessions. An epoch requires an unchanged full Docker container ID and cgroup path. It ends when
continuity cannot be established, including identity changes, non-increasing timestamps, material
limit changes, counter resets, or unavailable required metrics. Time gaps remain visible quality
signals: they mark an epoch as sparse and prohibit a time-to-limit estimate, but do not by themselves
end a strict-identity epoch.

An interval is sparse only when it exceeds twice the larger adjacent declared sampling interval. This
tolerates expected collection jitter without hiding material unobserved periods.

`memory.current` rate calculations require at least three observations across ten seconds with
strictly increasing timestamps. A time-to-limit estimate additionally requires continuous quality and
a stable finite `memory.max`. An unlimited limit does not suppress the current-memory trend.
Every semantic `memory.max` change closes an observation epoch.

An unavailable `memory.current` closes an epoch because its rate cannot cross an unknown point. An
unavailable or unlimited `memory.max` preserves the current-memory trend but makes a time-to-limit
estimate unavailable. No metric is silently skipped.

Overlapping evidence sessions for the same strict container identity are rejected as ambiguous input.
MemOptima does not deduplicate or choose between concurrent samples without an owner-approved rule.

When a container is absent from a selected session, its absence becomes an explicit observation gap.
It does not silently imply continuous observation or create a new identity. If the same strict
identity appears later, the gap contributes to sparse quality under the approved gap rule.

Version 0.7 reports a numeric rate and evidence quality. It does not classify a trend as stable,
rising, falling, anomalous, or risky before v0.8 evaluation establishes defensible thresholds.

Human-readable terminal tables retain the complete cgroup path and use an unambiguous Docker ID
prefix for each strict identity. A versioned JSON report is deferred to a later approved release,
after PSI, swap, cache, and intelligence-layer requirements define the complete reporting model.

The trend command returns exit code `0` for every valid report, including a report in which no epoch
has an available rate or time-to-limit estimate. It returns `1` for input or runtime failures and `2`
for command-usage failures.

The detailed timestamp, limit-change, counter-reset, and manifest-path rules are recorded in
`DECISIONS.md`. v0.7 processing streams its inputs and is limited to 128 manifest artifacts and
4,096 active container observation epochs. It returns a clear resource-limit result rather than
silently truncating a trend.

The manifest is a single versioned JSON document with a closed schema and an explicit artifact list.
Artifact paths are relative to the manifest directory and cannot escape its directory tree. Detailed
validation diagnostics remain pending owner approval. The manifest selects artifacts but does not set
their analytical order: MemOptima sorts them by validated session start time and rejects duplicates.
It fails closed when any selected artifact is unavailable, unsafe, malformed, or lifecycle-invalid.
Manifest version 1 is closed and contains only `manifest_version`, `artifacts`, and artifact `path`.

Manifest processing has two layers: a filesystem adapter safely reads one manifest file, then a pure
parser and validator maps its JSON text and source directory to a typed manifest model. CLI handlers
depend on the typed model and do not own manifest validation rules.

The public cross-session command is `memoptima trends analyze <MANIFEST>`. It writes one
human-readable table to standard output and provides no JSON output in v0.7.

## v0.8 forecast evaluation policy

The first v0.8 capability evaluates forecast accuracy from the same validated, manifest-selected
evidence model used for trends. `memoptima forecast evaluate <MANIFEST>` uses a strict container
identity composed of the full Docker ID and cgroup path. It fits only observations that occur before
the holdout and compares the resulting predicted `memory.current` with the next eligible actual
observation. It never uses future observations while fitting a forecast.

The evaluator reports eligible and evaluated counts, coverage, MAE, bias, status counts, and rows
for evaluated or ineligible epochs. Eligibility requires continuous historical observations, a
strictly increasing time series, available `memory.current`, and the existing rate minimums;
evaluation additionally requires a later actual `memory.current`. `memory.max` is not required
because this is usage evaluation rather than time-to-limit evaluation. Incomplete-session rows
remain visible but are excluded from aggregate quality metrics. Negative linear projections are
clamped to `0 B` before the evaluator reports them or calculates error.

Each usage epoch selects only its first eligible forecast attempt and uses the immediately following
observation as the holdout. If no such holdout exists, the eligible row remains unavailable for
error metrics. This keeps one stable row per epoch while making coverage meaningful instead of
selecting the final known observation and mechanically inflating it.

Evaluation inherits the manifest, artifact-reader, and bounded streaming safeguards. It has no more
than 4,096 result rows and fails without a partial report when a resource boundary is exceeded.
Version 0.8 initially exposes a human-readable table only. It does not make an OOM prediction,
confirm a memory leak, diagnose a defect, or alter a host or container.

The forecast-evaluation application service consumes the public trend catalog and evidence stream,
rechecks catalog-defining replay metadata, and returns either one complete report or a categorized
catalog, replay, or analyzer failure. CLI presentation is not part of this service.

Forecast-evaluation CLI presentation is human-first in v0.8: a metric summary precedes an
audit-oriented per-epoch table. It uses canonical UTC timestamps, full cgroup paths, explicit `N/A`
values, signed row error, and stable status text. A versioned machine-readable output remains a
separate future decision. `memoptima forecast evaluate <MANIFEST>` follows the established CLI
exit contract: valid reports return `0`, runtime or write failures return `1`, and usage failures
return `2`.

Each trend result represents one container observation epoch. Global summaries count result statuses
but never merge separate epochs into a single apparent trend.

Version 0.7 treats `memory.current` as the only primary trend metric. `memory.max` provides finite,
unlimited, stable, or changed limit context and eligibility; it is not itself a trend. PSI, swap, and
cache remain retained evidence for later, separately modeled intelligence capabilities.

## Result contracts

The trend command presents one human-readable table. It represents successful available and
valid-but-ineligible results without claiming an OOM prediction or confirmed memory leak. A
versioned JSON result format for automation and the future dashboard is deferred to a later release
and requires a separately approved public schema before implementation.

A successful analysis with no observation epochs writes an explicit message without a table header.
Trend windows and available time-to-limit results use an operational duration presentation in whole
seconds, minutes, hours, and days, with every displayed unit rounded down; the existing
microsecond-oriented duration display remains unchanged for other CLI output.

Trend tables present evidence quality independently from time-to-limit eligibility. Quality carries
both continuous or sparse observation coverage and incomplete-session provenance. Available rates
use right-aligned signed IEC units with two decimal places; display normalization prevents a negative
zero. Unavailable rates and time-to-limit values are explicit `N/A` values with one stable status
label explaining the limiting condition.

Trend tables retain the complete cgroup path and use dynamic widths bounded below by their headers.
Identity, quality, and status text is left-aligned; numeric and duration values are right-aligned.
Columns use two spaces between them and no trailing padding after the final status field.

## Safety boundary

The platform is currently observational and advisory. It must not make host, cgroup, container, or
workload changes. Any future action executor is a separate least-privilege component requiring a
new architecture decision, explicit approval, preconditions, expiry, outcome measurement, and a
documented rollback strategy.
