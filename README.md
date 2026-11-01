# MemOptima

MemOptima is an adaptive memory manager for Linux. Its first release will report system memory usage, identify memory-intensive processes, and classify memory pressure without modifying the system automatically.

## Current capabilities

- `memoptima --help` displays the available command-line options.
- `memoptima --version` displays the application version.

## Requirements

- Linux
- GCC with C++23 support
- CMake 3.20 or newer
- Ninja
- Internet access during the first configuration, so CMake can retrieve the pinned GoogleTest dependency

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

## Run

```bash
./build/debug/src/memoptima --help
./build/debug/src/memoptima --version
```

## License

MemOptima is distributed under the [MIT License](LICENSE).
