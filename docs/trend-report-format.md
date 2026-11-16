# Trend Report Format

## Purpose

`memoptima trend manifest <FILE>` reports cross-session `memory.current` behavior for each container
observation epoch. The default table is for terminal operators. JSON is the versioned machine-readable
contract.

## Table output

The table has one row per observation epoch. It includes an abbreviated container ID, epoch index,
observation count, window, largest gap, quality, current-memory rate, limit context, and time-to-limit
status when available. A summary counts epochs by quality and availability.

## JSON report version 1

```json
{
  "result_version": 1,
  "manifest": { "artifact_count": 2 },
  "summary": {
    "epoch_count": 1,
    "continuous_epoch_count": 1,
    "sparse_epoch_count": 0,
    "rate_available_epoch_count": 1,
    "time_to_limit_available_epoch_count": 1
  },
  "epochs": [
    {
      "container_id": "full-docker-container-id",
      "cgroup_path": "/sys/fs/cgroup/...",
      "epoch_index": 1,
      "first_observed_at_utc": "2026-01-01T12:00:00Z",
      "last_observed_at_utc": "2026-01-01T12:10:00Z",
      "observation_count": 3,
      "observation_window_ms": 600000,
      "largest_gap_ms": 10000,
      "quality": "continuous",
      "current_memory": {
        "first_bytes": 104857600,
        "last_bytes": 209715200,
        "rate_status": "available",
        "growth_bytes_per_second": 174762.666666667
      },
      "memory_max": { "status": "finite", "bytes": 536870912 },
      "time_to_memory_max": {
        "status": "available",
        "estimated_duration_ms": 1875000
      }
    }
  ]
}
```

`result_version` equals `1`. The root object contains `manifest`, `summary`, and `epochs`. The epoch
array is ordered by full container ID, cgroup path, and epoch index. This is stable presentation
order, not risk ranking.

## Epoch semantics

Each epoch retains the full Docker ID and cgroup path, canonical UTC endpoints, observation count,
window, largest gap, quality, current-memory result, limit context, and time-to-limit result.

`quality` is either `continuous` or `sparse`. A sparse epoch has a gap exceeding twice the larger
adjacent declared sampling interval. Sparse epochs cannot have an available time-to-limit estimate.

`current_memory.rate_status` is `available` only with at least three observations across ten seconds,
strictly increasing timestamps, and available current-memory values. Otherwise the rate is absent and
the status states the stable reason.

`memory_max.status` is `finite`, `unlimited`, or `unavailable`. Only a finite status has a `bytes`
value. `time_to_memory_max` is available only for continuous quality, a stable finite limit, and an
available rate satisfying the existing conservative v0.6 conditions.

## Safety and compatibility

JSON is a local audit artifact and deliberately retains full identifiers. Version 1 does not label an
epoch as stable, rising, falling, anomalous, risky, or leaking. It reports measured values and
availability conditions only.
