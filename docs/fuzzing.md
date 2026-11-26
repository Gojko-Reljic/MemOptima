# Fuzzing

MemOptima fuzzes untrusted-input boundaries with opt-in Clang libFuzzer targets. Normal Debug and
Release builds do not enable libFuzzer and remain unchanged.

## Strict JSON parser

Configure and build the fuzz preset:

```bash
cmake --preset fuzz
cmake --build --preset fuzz --target memoptima_strict_json_fuzz
```

Run the standard local smoke test for 60 seconds:

```bash
./build/fuzz/tests/fuzz/memoptima_strict_json_fuzz \
  -max_total_time=60 \
  -max_len=65536 \
  -malloc_limit_mb=512 \
  -dict=tests/fuzz/json.dict \
  build/fuzz/corpus \
  tests/fuzz/corpus/strict_json
```

The target checks semantic invariants as well as crashes and sanitizer findings. A successful strict
parse must match ordinary JSON parsing, survive a strict parse/dump/strict parse round trip, and not
exceed depth 64. An `invalid_json` result requires ordinary parsing to reject the input, while a
`duplicate_key` result requires ordinary parsing to accept it.

The corpus is also executed by ordinary CTest through `memoptima_strict_json_corpus_tests`. Any
minimized input discovered by libFuzzer must be added to `tests/fuzz/corpus/strict_json/` so it
becomes a permanent regression test.

The fuzz preset requires Clang and verifies support for the required libFuzzer flags during CMake
configuration. Scheduled longer fuzzing is deferred; it must not block normal pull-request checks.
