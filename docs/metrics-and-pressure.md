# MemOptima Metrics and Memory Pressure

This document explains the Linux data that MemOptima reads, the calculations it performs, and the limits of those calculations.

## Data sources

MemOptima reads system-wide memory values from `/proc/meminfo` and per-process memory values from `/proc/<pid>/status`.

- The [Linux kernel `/proc` documentation](https://docs.kernel.org/filesystems/proc.html) describes the system-wide memory fields.
- The [`proc_pid_status(5)` manual page](https://man7.org/linux/man-pages/man5/proc_pid_status.5.html) describes the per-process status fields.

MemOptima requires these `/proc/meminfo` values:

| Field | Meaning in MemOptima |
|---|---|
| `MemTotal` | Total usable RAM. |
| `MemAvailable` | The kernel estimate of memory available for new applications without swapping. |
| `SwapTotal` | Total configured swap space. |
| `SwapFree` | Unused configured swap space. |

The kernel reports these values in `kB`. MemOptima validates that unit and converts each value to bytes before performing calculations.

## RAM metrics

MemOptima displays these RAM values:

| Display label | Calculation |
|---|---|
| `Total` | `MemTotal` |
| `Available` | `MemAvailable` |
| `In use (estimate)` | `MemTotal - MemAvailable` |
| Used percentage | `(MemTotal - MemAvailable) / MemTotal * 100` |

`In use (estimate)` is deliberately labeled as an estimate. Linux uses RAM for page cache and other reclaimable resources, so a simple interpretation of unused physical pages is often misleading. `MemAvailable` is the kernel's estimate of memory that can support new applications without swapping, which is why MemOptima uses it instead of `MemFree`.

## Swap metrics

MemOptima displays these swap values:

| Display label | Calculation |
|---|---|
| `Total` | `SwapTotal` |
| `Free` | `SwapFree` |
| `In use` | `SwapTotal - SwapFree` |
| Used percentage | `(SwapTotal - SwapFree) / SwapTotal * 100` |

When no swap is configured, `SwapTotal` is zero. In that case, MemOptima displays `0 B (N/A)` for used swap and does not use swap as a memory-pressure signal.

## Process metric: VmRSS

The `processes` command reads `Name` and `VmRSS` from each readable `/proc/<pid>/status` file.

`VmRSS` is resident set size: memory currently resident in RAM for that process. The Linux manual page defines it as the sum of anonymous, file-backed, and shared-memory resident pages.

The process list is useful for identifying candidates for investigation, but it is not an exact ownership report:

- shared pages can contribute to more than one process;
- process memory changes while MemOptima scans `/proc`;
- Linux can deny access to another process's status file;
- the kernel documentation notes that `VmRSS` is not perfectly accurate.

For these reasons, do not sum the listed `VmRSS` values and expect the result to equal system-wide RAM usage. MemOptima reports the ten readable processes with the largest `VmRSS` values and notes how many records it skipped.

## Memory-pressure policy

MemOptima evaluates pressure from available RAM first, then considers swap usage.

### Available RAM level

| Available RAM as a percentage of total RAM | Base level | Reason |
|---|---|---|
| More than 20% | `Normal` | No RAM reason. |
| More than 10% and at most 20% | `Warning` | `Available RAM is at or below the warning threshold.` |
| At most 10% | `Critical` | `Available RAM is at or below the critical threshold.` |

The boundaries are inclusive on the more severe side. Exactly 20% is `Warning`; exactly 10% is `Critical`.

### Swap escalation

If at least 50% of configured swap is in use, MemOptima records this reason:

```text
Swap usage is at or above 50%.
```

High swap usage raises the base level by one step:

| Base level | Level with high swap usage |
|---|---|
| `Normal` | `Warning` |
| `Warning` | `Critical` |
| `Critical` | `Critical` |

### Examples

| Available RAM | Used swap | Result |
|---:|---:|---|
| 25% | 10% | `Normal` |
| 20% | 10% | `Warning` because the warning RAM boundary is inclusive. |
| 15% | 50% | `Critical` because high swap raises the RAM-based warning. |
| 10% | 0% | `Critical` because the critical RAM boundary is inclusive. |
| 30% | 60% | `Warning` because high swap raises normal pressure. |

## Interpretation limits

The pressure level is a deterministic policy, not a prediction of application failure or an out-of-memory event. It does not account for workload trends, memory cgroup limits, I/O latency, swappiness settings, or every Linux memory counter.

Use `Warning` and `Critical` as prompts to inspect current conditions. A useful next command is:

```bash
memoptima processes
```

MemOptima does not automatically close applications, change swap configuration, or otherwise modify the system.
