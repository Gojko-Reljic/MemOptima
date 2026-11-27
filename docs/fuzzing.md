# Fuzzing

MemOptima fuzzes untrusted-input boundaries with opt-in Clang libFuzzer targets. Normal Debug and
Release builds do not enable libFuzzer and remain unchanged.

## Strict JSON parser, evidence records, and terminal text

Configure and build the fuzz preset:

```bash
cmake --preset fuzz
cmake --build --preset fuzz --target memoptima_strict_json_fuzz
cmake --build --preset fuzz --target memoptima_evidence_record_fuzz
cmake --build --preset fuzz --target memoptima_terminal_text_fuzz
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

The evidence-record target applies the same process to `parse_evidence_record`, including its
strict JSON validation and schema validation:

```bash
./build/fuzz/tests/fuzz/memoptima_evidence_record_fuzz \
  -max_total_time=60 \
  -max_len=65536 \
  -malloc_limit_mb=512 \
  -dict=tests/fuzz/json.dict \
  build/fuzz/corpus \
  tests/fuzz/corpus/evidence_record
```

It asserts that a successfully parsed record has canonical JSON which succeeds when parsed again
and represents the same record type. The compiler-independent
`memoptima_evidence_record_corpus_tests` target executes every committed evidence-record seed in
ordinary CTest. The writable `build/fuzz/corpus` directory is intentionally separate from the
committed seed directories: inspect and minimize any newly discovered input before adding it as a
permanent regression case.

The terminal-text target fuzzes `escape_terminal_text` directly:

```bash
./build/fuzz/tests/fuzz/memoptima_terminal_text_fuzz \
  -max_total_time=60 \
  -max_len=65536 \
  -malloc_limit_mb=512 \
  build/fuzz/corpus \
  tests/fuzz/corpus/terminal_text
```

It verifies that every output is valid UTF-8 and contains no terminal-control code point forbidden
by ADR-047: C0, C1, DEL, bidi, zero-width, or line/paragraph separator characters. Its
`memoptima_terminal_text_corpus_tests` CTest target executes every committed seed.

The fuzz preset requires Clang and verifies support for the required libFuzzer flags during CMake
configuration. Scheduled longer fuzzing is deferred; it must not block normal pull-request checks.
