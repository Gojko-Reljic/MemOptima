# Architecture

## Current structure

MemOptima separates reusable application logic from its command-line interface.

```text
memoptima_core
        |
        +-- memoptima CLI
        |
        +-- unit tests
```

`memoptima_core` is an internal static library. The CLI and unit tests both link against it, so tests exercise the same code used by the application.

Memory collection, analysis, and terminal presentation have distinct responsibilities:

```text
MemorySnapshot
        |
        +-- linux/      reads and validates /proc/meminfo
        |
        +-- analysis/   derives memory and swap usage
        |
        +-- cli/        formats terminal text
```

## Planned evolution

The core library will keep logical areas in dedicated directories:

```text
memoptima_core
├── linux/       # Linux-specific data collection, including /proc/meminfo parsing
├── analysis/    # memory-pressure evaluation
└── ...
```

When those areas have stable interfaces and independent build needs, `memoptima_core` can be split into dedicated CMake libraries without changing the CLI contract.

## Build quality policy

- The project requires ISO C++23 and disables compiler-specific language extensions.
- GCC and Clang builds use `-Wall`, `-Wextra`, `-Wpedantic`, and `-Werror`.
- Debug builds enable AddressSanitizer and UndefinedBehaviorSanitizer.
- Debug and release configurations both build and run unit tests.
- GoogleTest is fetched at the fixed commit `063de7e9578f82b369302001269680b4b1553359`.
