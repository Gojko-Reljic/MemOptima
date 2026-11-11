# MemOptima User Guide

MemOptima is a Linux command-line tool for observing memory use. It reports RAM and swap usage, identifies memory-intensive processes, and explains the current memory-pressure assessment.

MemOptima does not automatically close processes, change swap settings, or modify other system settings.

## Requirements

To build MemOptima from source, install the following tools:

- Linux
- GCC with C++23 support
- CMake 3.20 or newer
- Ninja
- `clang-format` when tests are enabled
- Internet access during the first test-enabled CMake configuration, so CMake can download the pinned GoogleTest dependency

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

The Valgrind target checks `status`, `processes`, `containers`, invalid container-ID handling, and a
controlled `monitor` interruption. It fails on memory errors and definite or indirect leaks.

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
