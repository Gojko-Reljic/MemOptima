# Forecast Evaluation Report Format

## Purpose

`memoptima forecast evaluate <MANIFEST>` replays explicitly selected local evidence artifacts and
measures a historical `memory.current` projection against the next observation that was not used to
produce it. The command is read-only. It does not predict an OOM event, diagnose a memory leak, or
change a host, container, or cgroup.

The manifest has the same bounded, safe local-path contract as
[`memoptima trends analyze`](trend-manifest-format.md). Each selected artifact must be valid; one
catalog, replay, or analyzer failure fails the entire command without a partial report.

## Evaluation Method

An evaluation row is keyed by the strict container identity: full Docker container ID and cgroup
path. The evaluator considers only a continuous `memory.current` epoch with strictly increasing
capture times and available current-memory values.

For each compatible epoch, it selects the first usable history with at least three observations
covering at least ten seconds. It projects memory at the immediately following observation, which
is the holdout. The holdout value is never used in that projection. A negative
projection is clamped to zero bytes before it is displayed or used in metrics.

`memory.max` is not an input to this evaluation. A finite, unlimited, changed, or unavailable limit
does not itself interrupt a current-memory epoch.

## Summary Metrics

The report begins with the following values:

| Metric | Meaning |
|---|---|
| `ELIGIBLE` | Rows with sufficient continuous historical observations and a long enough window. |
| `EVALUATED` | Eligible rows with an immediately following usable holdout observation and a representable numeric result. |
| `COVERAGE` | `EVALUATED / ELIGIBLE`; `N/A` when no row is eligible. |
| `MAE` | Mean absolute error between predicted and actual `memory.current`, in IEC bytes. |
| `BIAS` | Mean signed error, `predicted - actual`, in IEC bytes. |
| `INCOMPLETE EXCLUSIONS` | Rows excluded because their source evidence session lacks `session_end`. |

The summary also lists every nonzero stable status count. Summary metrics are `N/A` when their
required evaluated values are unavailable.

## Table Output Contract

One row represents a container observation epoch. Columns are emitted in this order:

```text
CONTAINER ID  CGROUP PATH           EPOCH  HISTORY  WINDOW  FORECAST AT               HOLDOUT AT                PREDICTED    ACTUAL     ERROR  STATUS
a1b2c3d4e5f6  /docker/a1b2c3d4e5f6      1        3  10s     2026-01-01T00:00:10.000Z  2026-01-01T00:00:15.000Z  12.00 MiB  11.50 MiB  +512.00 KiB  evaluated
```

`CONTAINER ID`, `CGROUP PATH`, `FORECAST AT`, `HOLDOUT AT`, and `STATUS` are left-aligned.
`EPOCH`, `HISTORY`, `WINDOW`, `PREDICTED`, `ACTUAL`, and `ERROR` are right-aligned. Each width is
at least its header width and expands to the widest value. Columns have two separating spaces and
the final `STATUS` column has no trailing padding. Times use canonical UTC format
`YYYY-MM-DDTHH:MM:SS.mmmZ`.

`PREDICTED`, `ACTUAL`, and available summary errors use IEC byte units with two decimal places.
`ERROR` is `predicted - actual`, includes a sign, and normalizes a displayed negative zero to zero.
Unavailable fields are `N/A`. `WINDOW` uses the established operational-duration representation.

## Stable Statuses

| Status | Meaning |
|---|---|
| `evaluated` | A projection and its next observation were available and safely evaluated. |
| `excluded incomplete session` | The source session has no completed lifecycle record; the row is excluded from metrics. |
| `insufficient observations` | Fewer than three usable historical observations were available. |
| `insufficient observation window` | Usable historical observations covered less than ten seconds. |
| `container not observed continuously` | The strict identity was absent between samples. |
| `historical memory.current unavailable` | A required historical `memory.current` value was unavailable. |
| `non-increasing timestamps` | Capture times in the epoch were not strictly increasing. |
| `holdout observation unavailable` | No immediately following usable observation exists for an otherwise eligible row. |
| `numeric result unavailable` | A projection or error could not be represented safely. |

## Empty Analysis and Exit Codes

When analysis succeeds but finds no container observation epochs, the command writes:

```text
No forecast evaluation epochs found in manifest.
```

It writes no summary or table and returns `0`.

| Code | Meaning |
|---:|---|
| `0` | Evaluation and report output succeeded, including a valid empty report. |
| `1` | Manifest loading/parsing, catalog, evidence replay, analyzer, formatting, or output failed. |
| `2` | Command syntax or arguments are invalid. |

## Limits and Non-goals

The command accepts an explicit manifest rather than scanning directories. It preserves the manifest
limits of at most 128 artifacts and 4,096 active container observation epochs. It is an accuracy
measurement for this one conservative linear current-memory projection, not a comparison of all
forecast models.

No machine-readable evaluation report is emitted in v0.8. A versioned JSON contract is deferred
until upcoming PSI, swap, cache, and intelligence-layer requirements define a stable reporting
model.
