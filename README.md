# MemOptima

MemOptima is an adaptive memory manager for Linux. It reports system memory usage, identifies
memory-intensive processes, observes supported Docker cgroup memory, and retains bounded local
evidence without modifying the system automatically.

## Current capabilities

- `memoptima --help` displays the available command-line options.
- `memoptima --version` displays the application version.
- `memoptima status` displays current RAM and swap usage, memory-pressure reasons, and a safe recommendation.
- `memoptima monitor` continuously displays the memory status every five seconds until interrupted with `Ctrl+C`.
- `memoptima processes` displays the top ten processes by resident memory.
- `memoptima containers` displays memory summaries for supported rootful Docker containers.
- `memoptima container <FULL_ID>` displays the complete available cgroup memory snapshot for one Docker container.
- `memoptima assess containers` summarizes currently observable cgroup memory-boundary and OOM risk for supported rootful Docker containers.
- `memoptima assess container <FULL_ID>` explains the findings, evidence, and safe recommendations for one supported rootful Docker container.
- `memoptima sample containers --count <COUNT> --interval <DURATION> --output <NEW_FILE>` creates a bounded local JSONL evidence artifact from repeated Docker cgroup snapshots.
- `memoptima inspect evidence <FILE>` validates and summarizes a local evidence artifact.
- `memoptima forecast evidence <FILE>` estimates a cgroup `memory.max` crossing only from sufficient validated evidence.
- `memoptima trends analyze <MANIFEST>` analyzes validated memory-current trends across explicitly selected local evidence sessions.
- `memoptima forecast evaluate <MANIFEST>` measures eligible historical forecasts against their immediately following observed memory samples.

## Documentation

- [User Guide](docs/user-guide.md) explains how to build, test, and use MemOptima.
- [Metrics and Memory Pressure](docs/metrics-and-pressure.md) explains the Linux metrics, calculations, thresholds, and limitations.
- [MVP Scope](docs/mvp-scope.md) defines the current product boundary, safety and privacy behavior, and post-MVP direction.
- [0.2.0 Scope](docs/0.2.0-scope.md) defines the Docker-aware cgroup v2 and PSI observability release.
- [0.3.0 Scope](docs/0.3.0-scope.md) defines the read-only Docker cgroup memory assessment release.
- [0.4.0 Scope](docs/0.4.0-scope.md) defines the bounded sampling and retained-evidence release.
- [0.5.0 Scope](docs/0.5.0-scope.md) defines local evidence inspection and validation.
- [0.6.0 Scope](docs/0.6.0-scope.md) defines conservative evidence-based forecasting.
- [0.7.0 Scope](docs/0.7.0-scope.md) defines read-only cross-session memory trend analysis.
- [0.8.0 Scope](docs/0.8.0-scope.md) defines read-only temporal holdout evaluation of forecast accuracy.
- [0.9.0 Scope](docs/0.9.0-scope.md) defines terminal-safe rendering of artifact-derived paths and
  the public evidence-stream callback-finality contract.
- [Evidence Artifact Format](docs/evidence-artifact-format.md) defines the versioned JSON Lines schema.
- [Trend Manifest Format](docs/trend-manifest-format.md) defines the versioned JSON manifest that selects local evidence artifacts.
- [Forecast Evaluation Report Format](docs/forecast-evaluation-report-format.md) defines the evaluation table, metrics, and limitations.

## Requirements

- Linux
- GCC with C++23 support
- CMake 3.20 or newer
- Ninja
- `clang-format` when tests are enabled
- Internet access during the first configuration, so CMake can retrieve the pinned JSON dependency and, when tests are enabled, GoogleTest

## Build and test

Configure, build, and test the debug version:

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Configure, build, and test the optimized release version:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The debug preset enables AddressSanitizer and UndefinedBehaviorSanitizer. The release preset keeps tests enabled and disables sanitizers.

Run the Release command-line checks under Valgrind:

```bash
cmake --build --preset release --target memoptima_valgrind
```

This target checks `status`, `processes`, raw Docker container observation, both assessment command
paths, invalid container-ID handling, and a controlled `monitor` interruption for memory errors and
definite or indirect leaks.

## Run

```bash
./build/debug/src/memoptima --help
./build/debug/src/memoptima --version
./build/debug/src/memoptima status
./build/debug/src/memoptima monitor
./build/debug/src/memoptima processes
./build/debug/src/memoptima containers
./build/debug/src/memoptima container <FULL_ID>
./build/debug/src/memoptima assess containers
./build/debug/src/memoptima assess container <FULL_ID>
./build/debug/src/memoptima sample containers --count 3 --interval 5s --output evidence.jsonl
./build/debug/src/memoptima inspect evidence evidence.jsonl
./build/debug/src/memoptima forecast evidence evidence.jsonl
./build/debug/src/memoptima trends analyze manifest.json
./build/debug/src/memoptima forecast evaluate manifest.json
```

## Docker container memory

The Docker commands are local and read-only. They do not contact the Docker socket or Docker API,
do not invoke the Docker CLI, do not require `sudo`, and do not modify cgroup or kernel settings.

### Requirements and support boundary

The current Docker observability commands require:

- Linux with a unified cgroup v2 hierarchy;
- an available cgroup v2 `memory` controller;
- a readable host memory PSI interface;
- rootful Docker Engine using one of these standard cgroup v2 layouts:
  - `systemd`: `/sys/fs/cgroup/system.slice/docker-<full-id>.scope/`;
  - `cgroupfs`: `/sys/fs/cgroup/docker/<full-id>/`.

Rootless Docker, cgroup v1, Kubernetes discovery, Docker names/images/labels, continuous
monitoring, forecasts, automatic actions, and every system-changing action are outside the current
scope.

### List container summaries

Run:

```bash
./build/debug/src/memoptima containers
```

Example output (synthetic):

```text
Docker container memory

CONTAINER ID                                                      CURRENT       MEMORY.HIGH   MEMORY.MAX    OOM KILLS   PSI SOME AVG10
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  768.00 MiB    1.00 GiB      2.00 GiB      0           0.42%
```

`CURRENT` is `memory.current`. `MEMORY.HIGH` and `MEMORY.MAX` are the cgroup soft and hard
limits. `OOM KILLS` is the `memory.events` `oom_kill` counter. `PSI SOME AVG10` is the share of
the previous ten seconds in which at least one non-idle task was stalled for memory.

Rows are ordered by the full container ID. When no supported rootful Docker containers are found,
the command succeeds and prints `No supported rootful Docker containers found.`

### Inspect one container

Copy a full 64-character ID from `containers` and run:

```bash
./build/debug/src/memoptima container aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```

The detailed report groups identity and cgroup path, memory usage and limits, memory events,
anonymous and file-backed composition, current swap use, and complete cgroup memory PSI (`some`
and `full`, all averages and cumulative totals). PSI totals are displayed as readable durations.

`max` means that a cgroup limit is unlimited. `N/A` means that an individual metric could not be
read or validated. The detailed report then adds an `Unavailable data` section with the underlying
reason; repeated metrics with the same cause are reported once.

Discovery warnings are written to standard error so standard output remains usable in scripts. A
successful scan returns exit code `0`, even when it has warnings. Missing cgroup v2 capabilities or
a valid full ID that is not discovered return exit code `1`. A missing or invalid ID is a command
usage error and returns exit code `2`.

### Assess container memory risk

The assessment commands use the same local, read-only Docker cgroup discovery as the raw
observability commands. They do not contact Docker, change a cgroup limit, change a kernel setting,
or apply a remediation.

To assess all discovered containers, run:

```bash
./build/debug/src/memoptima assess containers
```

Example output (synthetic):

```text
Docker container assessments

CONTAINER ID                                                      RISK      COVERAGE   TOP FINDING                            FINDINGS
aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa  Critical  Complete   oom_kill_recorded                      2
```

`RISK` is the highest risk demonstrated by the available point-in-time cgroup data. It is one of
`Low`, `Moderate`, `High`, or `Critical`. `Low` means that no boundary or OOM signal covered by the
current rules was found; it does not mean that the workload is healthy or that future risk is absent.

`COVERAGE` is `Complete` when all evidence needed by the current rule set was available. It is
`Partial` when at least one metric could not be read or validated. Partial coverage never suppresses
a finding supported by other available data. `TOP FINDING` identifies the highest-priority finding;
`FINDINGS` gives the number of findings.

To inspect the assessment for one full container ID, run:

```bash
./build/debug/src/memoptima assess container aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
```

The detailed report lists each finding's stable code, risk level, raw evidence, and safe
recommendation. If coverage is partial, it also lists the unavailable evidence and its reading or
validation reason. The event counters used by the assessment are cumulative since the cgroup was
created, so a nonzero counter is historical evidence rather than proof of a recent event.

The assessment is intentionally not a forecast, leak diagnosis, workload-health certification, or
automatic recommendation engine. For the exact rule catalog and release boundary, see the
[0.3.0 Scope](docs/0.3.0-scope.md).

### Optional real Docker integration check

On a host with rootful Docker, cgroup v2, and permission to use the Docker CLI, run:

```bash
cmake --build --preset release --target memoptima_docker_integration
```

The target is never part of the normal test suite. It may pull the default `alpine:3.24.4` image,
starts one uniquely named and labeled temporary container, verifies the raw and assessment Docker
commands against its real cgroup, and removes only that container by its exact ID. Use a local
mirror or a different approved image with `-DMEMOPTIMA_DOCKER_TEST_IMAGE=<image>` while configuring
CMake.

## License

MemOptima is distributed under the [MIT License](LICENSE).
