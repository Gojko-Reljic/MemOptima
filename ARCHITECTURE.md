# MemOptima Architecture

## Authority and scope

This document is the canonical architecture reference for MemOptima. It describes the current v0.6
baseline and the approved direction toward v0.7. Product scope is defined in
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
| evidence validation | Safe input opening, schema and lifecycle validation | Forecast or recommendation policy |
| `forecast/` | Pure eligibility and forecasting calculations | Filesystem, Docker, JSON, or CLI |
| future `trends/` | Cross-session comparability, epochs, and trend calculations | Filesystem, JSON, or CLI |
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

v0.7 will introduce typed internal start, sample, and end records behind the existing validators.
The migration must preserve schema-v1 acceptance, safe input handling, and existing CLI behavior.

## Cross-session trend policy

Cross-session analysis is local and opt-in. It accepts a manifest that explicitly names the evidence
artifacts to analyze and must model an observation epoch before comparing container data across
sessions. An epoch requires an unchanged full Docker container ID and cgroup path. It ends when
continuity cannot be established, including identity changes, timestamp discontinuity, material limit
changes, counter resets, or unavailable required metrics.

The detailed timestamp, limit-change, counter-reset, and manifest-format rules remain pending owner
approval and must be recorded in `DECISIONS.md` before implementation. v0.7 processing streams its
inputs and is limited to 128 manifest artifacts and 4,096 active container observation epochs. It
must return a clear resource-limit result rather than silently truncate a trend.

## Result contracts

The trend command presents a human-readable table by default. It also provides a versioned JSON
result format for automation and the future dashboard. Both formats represent the same stable result
categories, including invalid input, valid-but-ineligible evidence, resource limits, and available
trends. The JSON schema must be documented and tested before implementation.

## Safety boundary

The platform is currently observational and advisory. It must not make host, cgroup, container, or
workload changes. Any future action executor is a separate least-privilege component requiring a
new architecture decision, explicit approval, preconditions, expiry, outcome measurement, and a
documented rollback strategy.
