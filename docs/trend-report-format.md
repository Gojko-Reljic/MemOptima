# Trend Report Format

## Purpose

`memoptima trends analyze <MANIFEST>` reads the explicitly selected local evidence artifacts named
by a manifest and reports cross-session `memory.current` behavior for every container observation
epoch. The command is read-only. It does not diagnose a memory leak, predict an OOM event, or change
the host, containers, or cgroups.

## Table Output Contract

One row represents one observation epoch. The columns are emitted in this order:

```text
CONTAINER ID  CGROUP PATH           EPOCH  SAMPLES  WINDOW  QUALITY                     RATE  TIME TO LIMIT  STATUS
a1b2c3d4e5f6  /docker/a1b2c3d4e5f6      1       12  1m 00s  continuous           +1.25 MiB/s        14m 20s  available
b7c8d9e0f1a2  /docker/b7c8d9e0f1a2      1        2     10s  sparse (incomplete)          N/A            N/A  sparse evidence
```

`CONTAINER ID`, `CGROUP PATH`, `QUALITY`, and `STATUS` are left-aligned. `EPOCH`, `SAMPLES`,
`WINDOW`, `RATE`, and `TIME TO LIMIT` are right-aligned. Every width is at least the matching header
width and expands to the longest value in the result. Columns use two spaces between them, and no
trailing padding follows `STATUS`. The full cgroup path is retained. Docker ID prefixes contain at
least twelve characters and extend as needed to remain unambiguous within one result.
Rows preserve the deterministic epoch order supplied by the analysis result.

## Column Semantics

`RATE` uses signed IEC units per second with two decimal places, such as `+1.25 MiB/s` or
`-512.00 KiB/s`. A displayed zero never has a negative sign. An unavailable rate is `N/A`.

`WINDOW` and available `TIME TO LIMIT` values use operational durations: `< 1s`, whole seconds,
`Xm YYs`, `Xh YYm`, or `Xd YYh`. Every displayed unit rounds down. An unavailable time-to-limit
value is `N/A`.

`QUALITY` is `continuous`, `sparse`, `continuous (incomplete)`, or `sparse (incomplete)`.

| STATUS | Meaning |
|---|---|
| `available` | A continuous epoch has a stable finite maximum, an available positive rate, and a representable estimate. |
| `insufficient observations` | Fewer than three eligible observations are available for a rate. |
| `insufficient observation window` | Eligible observations span fewer than ten seconds. |
| `sparse evidence` | An observation gap exceeds the approved continuity threshold. |
| `non-positive growth` | The available rate is zero or negative. |
| `unlimited memory.max` | `memory.max` is unlimited, so no limit-crossing estimate is applicable. |
| `unavailable memory.max` | `memory.max` is unavailable, so no limit-crossing estimate is available. |
| `at or above memory.max` | The latest observed memory is already at or above the finite maximum. |
| `estimate out of range` | The calculated duration cannot be represented safely. |

## Empty Analysis

When analysis succeeds but finds no container observation epochs, the command writes:

```text
No container observation epochs found in manifest.
```

It writes no table header and returns exit code `0`.

## Exit Codes

| Code | Meaning |
|---:|---|
| `0` | Analysis and table output succeeded, including valid but unavailable or empty results. |
| `1` | Manifest loading or parsing, catalog, evidence replay, analyzer processing, or table output failed. |
| `2` | Command syntax or arguments are invalid. |

## Future JSON Contract

A machine-readable, versioned JSON report format is deferred to v0.8 to incorporate upcoming memory
signals: PSI, swap, and cache.
