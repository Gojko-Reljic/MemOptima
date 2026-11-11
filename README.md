# MemOptima

MemOptima is an adaptive memory manager for Linux. Its first release will report system memory usage, identify memory-intensive processes, and classify memory pressure without modifying the system automatically.

## Current capabilities

- `memoptima --help` displays the available command-line options.
- `memoptima --version` displays the application version.
- `memoptima status` displays current RAM and swap usage, memory-pressure reasons, and a safe recommendation.
- `memoptima monitor` continuously displays the memory status every five seconds until interrupted with `Ctrl+C`.
- `memoptima processes` displays the top ten processes by resident memory.
- `memoptima containers` displays memory summaries for supported rootful Docker containers.
- `memoptima container <FULL_ID>` displays the complete available cgroup memory snapshot for one Docker container.

## Documentation

- [User Guide](docs/user-guide.md) explains how to build, test, and use MemOptima.
- [Metrics and Memory Pressure](docs/metrics-and-pressure.md) explains the Linux metrics, calculations, thresholds, and limitations.
- [MVP Scope](docs/mvp-scope.md) defines the current product boundary, safety and privacy behavior, and post-MVP direction.
- [0.2.0 Scope](docs/0.2.0-scope.md) defines the planned Docker-aware cgroup v2 and PSI observability release.

## Requirements

- Linux
- GCC with C++23 support
- CMake 3.20 or newer
- Ninja
- `clang-format` when tests are enabled
- Internet access during the first test-enabled configuration, so CMake can retrieve the pinned GoogleTest dependency

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

This target checks `status`, `processes`, and a controlled `monitor` interruption for memory errors and definite or indirect leaks.

## Run

```bash
./build/debug/src/memoptima --help
./build/debug/src/memoptima --version
./build/debug/src/memoptima status
./build/debug/src/memoptima monitor
./build/debug/src/memoptima processes
./build/debug/src/memoptima containers
./build/debug/src/memoptima container <FULL_ID>
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
monitoring, recommendations, and every system-changing action are outside the current scope.

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

## License

MemOptima is distributed under the [MIT License](LICENSE).
