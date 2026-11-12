# MemOptima Evidence Artifact Format

MemOptima 0.4.0 writes one local JSON Lines (`.jsonl`) evidence artifact for one explicit sampling
session. JSON Lines means that every physical line is one complete JSON object. Earlier complete
lines remain readable if a later interruption prevents a final line.

Every record has `record_type` and `schema_version`. The current schema version is `1`.
Record keys have a stable writer order, but consumers must select fields by key name.

## Record lifecycle

A normally completed artifact has this order:

```text
session_start
sample
sample
...
session_end
```

There is one `sample` for every completed collection attempt. An empty `containers` array is valid:
it means no supported rootful Docker cgroup was discovered at that instant. A missing `session_end`
means the artifact is incomplete and must not be treated as a normally closed session.

## Value conventions

- Timestamps are UTC ISO 8601 values with millisecond precision, for example
  `2026-11-13T14:30:15.123Z`.
- Unsigned 64-bit byte values, counters, PSI totals, and durations are decimal JSON strings.
- PSI averages are JSON numbers from `0` through `100`.
- An unreadable metric is `null`; its source, stable code, and detail are in `unavailable_metrics`.
- A literal cgroup limit of `max` is the JSON string `"max"`, not `null`.

## `session_start`

The first record contains the application version, requested count and interval, 512 MiB output
limit, required `0600` permissions, the per-record flush contract, and
`host_identifiers_included: false`.

## `sample`

Every sample contains a one-based `sample_sequence`, `planned_offset_ms`, `captured_at_utc`, and
`schedule_delay_ms`. The delay measures how late collection began relative to its fixed-rate
deadline; it does not invent skipped collection attempts.

Each container contains `container_id`, `cgroup_path`, `memory`, and `assessment`. `memory` keeps
the raw cgroup snapshot: current memory, `memory.high`, `memory.max`, events, anonymous and
file-backed composition, swap use, complete PSI `some` and `full`, plus unavailable metrics. The
assessment keeps its risk, coverage, findings, evidence, recommendations, and unavailable evidence.

`discovery_warnings` remains in the matching sample. Each item has `code`, `cgroup_path`, and
`detail`.

## `session_end`

The terminal record has `outcome`, `completed_sample_count`, `ended_at_utc`, and `failure`.

| Outcome | `failure` | Meaning |
|---|---|---|
| `completed` | `null` | All requested samples were written. |
| `interrupted` | `null` | The user stopped the session with `Ctrl+C`. |
| `failed` | object | The artifact could not complete after creation. |

Failure codes are `output_size_limit_reached`, `output_write_failed`, and `output_flush_failed`.
If an I/O problem prevents the terminal record itself, its absence marks the artifact incomplete.

## Privacy and durability

The artifact contains no hostname, user name, IP address, machine ID, or derived host identifier.
It necessarily retains Docker container IDs and cgroup paths to identify observed workloads.

Every complete record is flushed before the next attempt. This does not promise physical disk
persistence after abrupt power loss. The artifact retains observations and point-in-time
assessments; it does not assert a trend, forecast, leak diagnosis, workload health, or action.
