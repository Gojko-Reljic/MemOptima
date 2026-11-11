# MemOptima

MemOptima is an adaptive memory manager for Linux. Its first release will report system memory usage, identify memory-intensive processes, and classify memory pressure without modifying the system automatically.

## Current capabilities

- `memoptima --help` displays the available command-line options.
- `memoptima --version` displays the application version.
- `memoptima status` displays current RAM and swap usage, memory-pressure reasons, and a safe recommendation.
- `memoptima monitor` continuously displays the memory status every five seconds until interrupted with `Ctrl+C`.
- `memoptima processes` displays the top ten processes by resident memory.

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
```

## License

MemOptima is distributed under the [MIT License](LICENSE).
