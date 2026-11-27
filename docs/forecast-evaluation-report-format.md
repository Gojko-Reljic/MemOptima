# Forecast Evaluation Report Format

## Purpose

`memoptima forecast evaluate <MANIFEST>` replays explicitly selected local evidence artifacts and
evaluates the shared linear `memory.current` model at fixed horizons: `1m`, `5m`, `15m`, and `1h`.
It is read-only. It does not predict OOM events, diagnose memory leaks, recommend limits, or change
a host, container, or cgroup.

## Rolling-Origin Method

An origin requires at least three continuous observations spanning ten seconds. The model receives
only observations at or before that origin. For every horizon, the evaluator selects the nearest
same-epoch holdout whose elapsed time is within
`min(requested sampling interval / 2, horizon / 10)`, then projects to that holdout's actual time.
No evaluation crosses a session boundary.

An epoch keeps at most 256 origins through deterministic streaming decimation. The first eligible
origin is retained. Each epoch and horizon needs at least three evaluated origins for metrics.

## Table Output Contract

Rows sort by full container identity, epoch, and numeric horizon duration. An epoch that cannot be
forecast receives one row with `HORIZON` set to `N/A`; otherwise it has one row per horizon.

```text
CONTAINER ID  CGROUP PATH       EPOCH  HORIZON  ORIGINS  MAE         BIAS       STATUS
a1b2c3d4e5f6  /docker/example       1  1m 00s        12  1.25 MiB   +0.25 MiB  evaluated
```

Text columns are left-aligned and numeric columns are right-aligned. A width is never less than its
header. Columns use two spaces, and `STATUS` has no trailing padding. Byte values use IEC units and
two decimals. `N/A` represents an unavailable value.

## Horizon Aggregates

For each horizon the summary reports equally weighted qualifying epochs:

| Metric | Meaning |
|---|---|
| `MAE` | Mean epoch model absolute error in bytes. |
| `persistence MAE` | Mean epoch error of predicting the origin value unchanged. |
| `bias` | Mean epoch signed error, predicted minus actual. |
| `relative MAE` | Mean epoch error relative to memory used at each origin, with a 1 MiB denominator floor. |
| `skill` | `1 - geometric mean(model MAE / persistence MAE)`. Positive is better than persistence. |

Epochs with zero persistence error remain in byte and relative metrics. They are excluded from
skill and separately counted as `persistence exact/model exact` or `persistence exact/model error`.
Skill is `N/A` when no epoch can contribute.

## Stable Statuses

| Status | Meaning |
|---|---|
| `evaluated` | The epoch and horizon have at least three valid paired origins. |
| `excluded incomplete session` | Source evidence lacks a completed session lifecycle. |
| `insufficient observations` | Fewer than three observations exist in the epoch. |
| `insufficient observation window` | Observations span less than ten seconds. |
| `container not observed continuously` | The identity is absent between samples. |
| `historical memory.current unavailable` | A required current-memory value is unavailable. |
| `non-increasing timestamps` | Epoch timestamps are not strictly increasing. |
| `horizon below sampling interval` | The requested sampling interval is longer than that horizon. |
| `holdout observation unavailable` | No compatible same-epoch holdout exists. |
| `insufficient origins` | Fewer than three origins have compatible holdouts. |
| `numeric result unavailable` | A model projection cannot safely be represented. |

## Empty Analysis and Exit Codes

An otherwise valid empty analysis prints `No forecast evaluation epochs found in manifest.` and
returns `0`. Exit code `1` means loading, replay, analysis, formatting, or output failed. Exit code
`2` means invalid command syntax.

## Limits and Non-goals

The evaluator accepts the manifest's maximum 128 artifacts and bounds active epochs to 1,024,
result epochs to 4,096, and result rows to the epoch limit times the number of horizons. A limit
failure is atomic; no partial report is emitted. JSON evaluation output, configurable horizons,
time-to-limit accuracy, confidence intervals, anomaly detection, and leak detection are deferred.
