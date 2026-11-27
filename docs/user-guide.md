# MemOptima User Guide

MemOptima is a Linux command-line tool for observing memory use. It reports RAM and swap usage,
identifies memory-intensive processes, observes supported Docker cgroup memory data, and assesses
currently observable cgroup memory-boundary and OOM signals. It can also retain a bounded sequence
of Docker cgroup snapshots as an explicit local evidence artifact.

MemOptima does not automatically close processes, change swap settings, or modify other system settings.

## Requirements

To build MemOptima from source, install the following tools:

- Linux
- GCC with C++23 support
- CMake 3.20 or newer
- Ninja
- `clang-format` when tests are enabled
- Internet access during the first CMake configuration, so CMake can download the pinned JSON dependency and, when tests are enabled, GoogleTest

## Build from source

Open a terminal in the project root. The project root is the directory that contains `CMakeLists.txt` and `CMakePresets.json`.

Configure and build the debug version:

```bash
cmake --preset debug
cmake --build --preset debug
```

The debug preset enables AddressSanitizer and UndefinedBehaviorSanitizer. These tools help detect memory-safety and undefined-behavior problems while developing the project.

Run the debug test suite:

```bash
ctest --preset debug
```

To build the optimized release version instead:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Both presets build and run the test suite. The release preset disables sanitizers.

Run the Release command-line checks under Valgrind:

```bash
cmake --build --preset release --target memoptima_valgrind
```

The Valgrind target checks `status`, `processes`, raw Docker container observation, both assessment
command paths, invalid container-ID handling, and a controlled `monitor` interruption. It fails on
memory errors and definite or indirect leaks.

## Install

Build the release version before installing it:

```bash
cmake --preset release
cmake --build --preset release
```

Install into the standard per-user prefix without `sudo`:

```bash
cmake --install build/release --prefix "$HOME/.local"
```

This installs the executable at `$HOME/.local/bin/memoptima` and the documentation below `$HOME/.local/share/doc/MemOptima/`.

Check whether the executable directory is already in `PATH`:

```bash
printf '%s\n' "$PATH"
```

If `$HOME/.local/bin` is not listed, make it available for the current terminal session:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

This command does not change shell configuration files. After the directory is in `PATH`, run MemOptima as `memoptima` instead of using its build-directory path.

## Run MemOptima

After a debug build, the executable is located at:

```text
./build/debug/src/memoptima
```

Replace `debug` with `release` when using the release build.

### Display help and version

```bash
./build/debug/src/memoptima --help
./build/debug/src/memoptima --version
```

The help command lists every supported option. The version command prints the application name and version.

### Inspect current memory status

```bash
./build/debug/src/memoptima status
```

Example output:

```text
Memory status

RAM
  Total:              31.23 GiB
  Available:          25.77 GiB
  In use (estimate): 5.46 GiB (17.5%)

Swap
  Total:              8.00 GiB
  Free:               8.00 GiB
  In use:             0 B (0.0%)

Memory pressure
  Level:              Normal
  Recommendation:     No action is required.
```

Actual values depend on the current system state. A `Warning` or `Critical` result includes one or more `Reason:` lines before the recommendation.

### Inspect memory-intensive processes

```bash
./build/debug/src/memoptima processes
```

Example output:

```text
Top memory processes (VmRSS)

PID     NAME                 RESIDENT MEMORY
1234    example-process      256.00 MiB
```

MemOptima displays up to ten readable processes, ordered by resident memory. A note about skipped processes means that Linux did not allow MemOptima to read one or more process status files, or that a process ended during the scan.

### Continuously monitor memory status

```bash
./build/debug/src/memoptima monitor
```

The first measurement appears immediately. Later measurements appear every five seconds and begin with a UTC ISO 8601 timestamp:

```text
2026-11-10T13:05:00Z

Memory status
...
```

Press `Ctrl+C` to stop monitoring normally. MemOptima returns exit code `0` after this expected interruption.

You can save successful measurements while keeping errors separate:

```bash
./build/debug/src/memoptima monitor > monitor.log 2> monitor-errors.log
```

## Assess Docker container memory risk

MemOptima can assess supported rootful Docker containers that use cgroup v2. This is a local,
read-only analysis feature: it does not contact the Docker socket or Docker API, invoke the Docker
CLI, require `sudo`, modify a cgroup, or change a Linux memory-management setting.

The feature requires:

- Linux with a unified cgroup v2 hierarchy;
- an available cgroup v2 `memory` controller;
- a readable host memory PSI interface; and
- rootful Docker Engine using either the `systemd` or `cgroupfs` cgroup v2 layout.

Rootless Docker, cgroup v1, Kubernetes workload discovery, continuous assessment, automatic
recommendations, and automatic action are not supported by this release.

## Sample Docker container memory evidence

Use `sample containers` to retain a bounded local record of repeated Docker cgroup snapshots:

```bash
./build/debug/src/memoptima sample containers --count 12 --interval 5s --output evidence.jsonl
```

All three options are required. `--count` accepts `1` through `3600`. `--interval` accepts a
canonical duration from `100ms` through `60m`, using `ms`, `s`, or `m`. `--output` must name a new
path: MemOptima never overwrites or appends to an existing artifact.

The first sample is immediate. Later samples follow a monotonic fixed-rate schedule, so wall-clock
changes do not change cadence. A slow collection never starts concurrent work or silently skips an
attempt; its delay is retained in the sample record.

Before it creates the artifact, MemOptima validates the arguments and checks cgroup v2, the memory
controller, and host memory PSI. It creates the exact requested path with owner-only `0600`
permissions. The artifact is limited to 512 MiB and reserves terminal-record space before another
sample is written.

Sampling remains quiet on standard output. It writes a final standard-error summary such as:

```text
Sampling session completed: 12 sample(s), artifact: evidence.jsonl
```

Press `Ctrl+C` to stop normally. Earlier complete records remain valid, and MemOptima writes an
`interrupted` terminal record whenever possible before returning exit code `0`.

### Read the JSON Lines artifact

Each line is one JSON object: the first line is `session_start`, every collection is `sample`, and a
normal final line is `session_end`. For a quick inspection with `jq`:

```bash
jq -c '.record_type' evidence.jsonl
jq -c 'select(.record_type == "sample") | {sample_sequence, captured_at_utc, containers: (.containers | length)}' evidence.jsonl
```

The artifact retains raw metrics and assessments together. 64-bit bytes, counters, PSI totals, and
durations are decimal strings; PSI averages are numbers. `"max"` means an unlimited cgroup limit.
`null` means a metric was unavailable, with its reason in `unavailable_metrics`. An empty
`containers` list is a successful collection with no supported rootful Docker cgroup.

See [Evidence Artifact Format](evidence-artifact-format.md) for the complete versioned schema.

### Inspect a retained artifact

```bash
./build/debug/src/memoptima inspect evidence evidence.jsonl
```

The reader accepts only an existing regular file that is not a symbolic link. It limits both one
physical line and the aggregate physical input to 512 MiB, validates the schema version and record
lifecycle, then prints a human-readable session and container summary. An artifact without
`session_end` remains readable but is clearly marked `Incomplete`; it does not claim a trend,
forecast, diagnosis, health result, recommendation, or action.

### Forecast a retained artifact

```bash
./build/debug/src/memoptima forecast evidence evidence.jsonl
```

Forecasting reads the same explicit local artifact and validates it before analysis. A container
needs at least three observations across ten seconds, continuous presence in every sample, strictly
increasing capture times, and a stable finite `memory.max`. The estimate is time to that cgroup
limit based on the latest observed memory and a linear growth trend. It is not an OOM prediction,
memory-leak diagnosis, health result, recommendation, or action. An unlimited `memory.max` is
reported as not applicable; incomplete or insufficient evidence receives an explicit reason.

### Analyze trends across selected sessions

Use a manifest to select the evidence artifacts to analyze. The manifest is one JSON document whose
artifact paths are relative to the manifest's directory:

```json
{
  "manifest_version": 1,
  "artifacts": [
    { "path": "evidence/session-001.jsonl" },
    { "path": "evidence/session-002.jsonl" }
  ]
}
```

Run the analysis with:

```bash
./build/debug/src/memoptima trends analyze manifest.json
```

The command validates every selected artifact, then reports a separate `memory.current` trend for
each compatible container observation epoch. Artifact paths are opened beneath the manifest directory
without following symbolic links. It is read-only and does not scan directories, diagnose a memory
leak, predict an OOM event, or change a host, container, or cgroup. A valid analysis with no
container epochs is still successful and prints a clear empty-result message.

See [Trend Manifest Format](trend-manifest-format.md) for the input contract and
[Trend Report Format](trend-report-format.md) for the table, statuses, and limitations.

### Evaluate historical forecast accuracy

Forecast evaluation replays the explicitly selected artifacts from a trend manifest and evaluates
the shared linear `memory.current` projection at 1m, 5m, 15m, and 1h horizons. It is a rolling-origin
holdout measurement: a projection uses only data available at its origin.

Run the evaluation with:

```bash
./build/debug/src/memoptima forecast evaluate manifest.json
```

For a continuous container observation epoch, each origin requires at least three observations
spanning at least ten seconds. A same-epoch holdout must be close enough to the requested horizon;
rows whose evidence is incomplete, discontinuous, too short, sampled too slowly, or missing a
compatible holdout remain visible with an explicit status.

The report includes per-horizon mean absolute error, persistence-baseline error, signed bias,
relative error, and skill. It evaluates `memory.current` only. It does not predict an OOM event,
diagnose a memory leak, certify workload health, recommend a limit, or modify a host, container,
or cgroup.

See [Trend Manifest Format](trend-manifest-format.md) for the shared input contract and
[Forecast Evaluation Report Format](forecast-evaluation-report-format.md) for the output, stable
statuses, and limitations.

### Privacy, durability, and limits

The artifact does not contain hostname, user name, IP address, machine ID, or a derived host
identifier. It necessarily includes full Docker container IDs and cgroup paths to identify the
observed workload. Each complete record is flushed before the next attempt, but this is not a
guarantee against abrupt power loss. A missing `session_end` means the artifact is incomplete.

Sampling preserves evidence for inspection and conservative limit forecasting. It does not predict
an OOM event, diagnose a leak, certify workload health, make an automatic recommendation, or change
the host or a workload.

### Assess all discovered containers

Run:

```bash
./build/debug/src/memoptima assess containers
```

Example output (synthetic):

```text
Docker container assessments

CONTAINER ID                                                      RISK      COVERAGE   TOP FINDING                            FINDINGS
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  High      Partial    memory_max_event_recorded              1
```

Each row represents one discovered supported Docker cgroup.

- `RISK` is the highest risk level currently demonstrated by a finding: `Low`, `Moderate`, `High`,
  or `Critical`.
- `COVERAGE` is `Complete` when every metric required by the current rules was available. It is
  `Partial` when at least one required metric could not be read or validated.
- `TOP FINDING` is the highest-priority finding after the deterministic risk and code ordering.
- `FINDINGS` is the total number of findings for that container.

If no supported rootful Docker containers are discovered, the command succeeds and prints:

```text
No supported rootful Docker containers found.
```

An empty result does not indicate an error. Discovery warnings are written to standard error, so
standard output remains usable in scripts.

### Assess one container in detail

Copy the full 64-character ID from `containers` or `assess containers`, then run:

```bash
./build/debug/src/memoptima assess container aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```

The result has these sections:

- `Container` identifies the full Docker ID and discovered cgroup path.
- `Overall risk` reports the highest demonstrated risk among all findings.
- `Coverage` shows whether all rule evidence was available.
- `Findings` lists each finding with its stable code, risk, evidence, and a fixed safe
  recommendation.
- `Unavailable evidence` appears only for partial coverage. It gives the source metric and the
  reason it could not be read or validated.

For example, an `oom_kill_recorded` finding is `Critical` because the cgroup's
`memory.events.oom_kill` counter is nonzero. The report states the observed counter value and asks
the operator to inspect workload recovery and cgroup memory settings; it never makes that change
automatically.

### What the assessment means

The assessment evaluates one cgroup snapshot. It currently uses finite `memory.max` and
`memory.high` boundaries plus the `memory.events` `high`, `max`, `oom`, and `oom_kill` counters.
The exact rules and threshold boundaries are documented in [the 0.3.0 scope](0.3.0-scope.md).

`Low` is deliberately narrow: it means that the available snapshot produced no finding under these
rules. It does not prove that a workload is healthy, that its memory use is stable, or that it cannot
fail in the future.

Event counters are cumulative since the cgroup was created. A nonzero event counter is useful
historical evidence, but it does not establish when the event happened or whether its value is
increasing. MemOptima therefore does not call it a trend or make a forecast.

When coverage is `Partial`, MemOptima still reports every finding proven by available evidence. It
does not treat missing information as safe. Read the `Unavailable evidence` section before making an
operational decision.

## Exit codes

| Code | Meaning |
|---:|---|
| `0` | The command completed successfully. |
| `1` | A runtime or system-reading error occurred. |
| `2` | The command-line usage was invalid. |

## Troubleshooting

### CMake cannot find a required tool

Confirm that the required commands are available:

```bash
g++ --version
cmake --version
ninja --version
```

Install the missing tool using your Linux distribution's package manager, then configure the project again.

### A preset command cannot find the project

Run `cmake --preset ...` from the project root, not from `src/`, `tests/`, or a build directory. The root contains `CMakePresets.json`.

### The first configuration cannot download GoogleTest

The first CMake configuration downloads a pinned GoogleTest revision. Confirm that the computer has internet access, then run the configuration command again.

### The process list reports skipped processes

This can be normal. Linux may prevent reading another process's status information, and processes can end while MemOptima is scanning `/proc`. MemOptima continues with the readable process records.

### The monitor does not stop immediately

Use `Ctrl+C`. MemOptima checks for this interruption at least every 100 milliseconds while waiting for the next measurement.

### Sampling cannot create its output artifact

Choose a new `--output` path. MemOptima intentionally refuses to overwrite or append to an existing
evidence file. Also verify that the target directory exists and that your account can create an
owner-only file there.
