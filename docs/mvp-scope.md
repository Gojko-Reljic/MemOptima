# MemOptima MVP Scope

This document defines the product boundary for the MemOptima minimum viable product (MVP). It distinguishes implemented behavior from deliberate non-goals and possible future directions.

## What the MVP does

The MVP is a Linux command-line tool that observes local memory conditions.

It provides these commands:

| Command | Purpose |
|---|---|
| `memoptima status` | Displays current RAM and swap data, memory-pressure reasons, and a safe recommendation. |
| `memoptima processes` | Displays up to ten readable processes with the largest `VmRSS` values. |
| `memoptima monitor` | Repeats the memory status every five seconds until the user stops it with `Ctrl+C`. |
| `memoptima --help` | Displays supported command-line options. |
| `memoptima --version` | Displays the application version. |

The MVP evaluates memory pressure with fixed, documented thresholds. It distinguishes `Normal`, `Warning`, and `Critical` results, and explains the active reasons when a threshold is reached.

## What the MVP does not do

The MVP does not:

- terminate, pause, start, or otherwise control processes;
- change swap configuration, kernel parameters, or memory limits;
- automatically reclaim memory or apply an optimization;
- run as a background daemon or system service;
- provide a graphical user interface;
- store monitoring history, configuration, or process data;
- provide JSON, an HTTP API, or integration with an external monitoring service;
- allow custom memory-pressure thresholds;
- guarantee that every process can be read from `/proc`.

These exclusions are intentional. The MVP reports conditions and offers safe guidance, but the user remains responsible for every system-changing action.

## Safety and privacy boundaries

At runtime, MemOptima reads local Linux `/proc` files. It does not send runtime data over the network and does not contact an external service.

The process command can display local process identifiers, process names, and resident-memory values. The monitor command can display time-stamped memory status repeatedly.

MemOptima does not create log files itself. If a user redirects output to a file, such as with `> monitor.log`, that file is created and managed by the user and may contain local process or system-memory information.

The first CMake configuration may access the network to download the pinned GoogleTest dependency for building tests. This build-time dependency retrieval is separate from the behavior of the MemOptima executable.

## Known limitations

- Linux memory values are snapshots and can change while MemOptima is reading them.
- `MemAvailable` is a kernel estimate, so `In use (estimate)` is not a direct measurement of exclusive application ownership.
- `VmRSS` can include shared pages and is not an exact per-process ownership value.
- Linux permission rules and process lifetime changes can make some process status files unreadable during a scan.
- The pressure policy uses fixed thresholds and does not consider workload trends, memory cgroups, I/O latency, or all kernel memory counters.
- The MVP requires `MemAvailable`; it does not implement a fallback for older Linux kernels that do not provide that field.

## Post-MVP direction

The following are possible directions, not promised features or delivery dates:

- configurable monitor intervals and pressure thresholds;
- structured output such as JSON for scripts and external monitoring tools;
- richer process metrics, including more precise memory-accounting options;
- historical measurements and memory-pressure trends;
- an optional graphical interface or background daemon;
- carefully designed, explicitly approved automated actions with safety checks.

Any future system-changing capability must remain opt-in, explain its intended effect, and provide a safe way for the user to retain control.
