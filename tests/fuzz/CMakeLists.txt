add_executable(memoptima_strict_json_corpus_tests
    strict_json_corpus_runner.cpp
    strict_json_invariants.cpp
)

target_include_directories(memoptima_strict_json_corpus_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(memoptima_strict_json_corpus_tests
    PRIVATE
        memoptima_build_options
        memoptima_core
        nlohmann_json::nlohmann_json
)

set_target_properties(memoptima_strict_json_corpus_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(
    NAME MemOptimaFuzzCorpus.StrictJson
    COMMAND memoptima_strict_json_corpus_tests
        "${PROJECT_SOURCE_DIR}/tests/fuzz/corpus/strict_json"
)

add_executable(memoptima_evidence_record_corpus_tests
    evidence_record_corpus_runner.cpp
    evidence_record_invariants.cpp
)
target_link_libraries(memoptima_evidence_record_corpus_tests
    PRIVATE
        memoptima_build_options
        memoptima_core
)
set_target_properties(memoptima_evidence_record_corpus_tests PROPERTIES
    CXX_EXTENSIONS OFF
)
add_test(
    NAME MemOptimaFuzzCorpus.EvidenceRecord
    COMMAND memoptima_evidence_record_corpus_tests
        "${PROJECT_SOURCE_DIR}/tests/fuzz/corpus/evidence_record"
)

add_executable(memoptima_terminal_text_corpus_tests
    terminal_text_corpus_runner.cpp
    terminal_text_invariants.cpp
)
target_include_directories(memoptima_terminal_text_corpus_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)
target_link_libraries(memoptima_terminal_text_corpus_tests
    PRIVATE
        memoptima_build_options
        memoptima_cli_support
)
set_target_properties(memoptima_terminal_text_corpus_tests PROPERTIES
    CXX_EXTENSIONS OFF
)
add_test(
    NAME MemOptimaFuzzCorpus.TerminalText
    COMMAND memoptima_terminal_text_corpus_tests
        "${PROJECT_SOURCE_DIR}/tests/fuzz/corpus/terminal_text"
)

if(MEMOPTIMA_BUILD_FUZZERS)
    file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/corpus")

    add_executable(memoptima_strict_json_fuzz
        "${PROJECT_SOURCE_DIR}/src/strict_json_parser.cpp"
        strict_json_fuzz.cpp
        strict_json_invariants.cpp
    )

    target_include_directories(memoptima_strict_json_fuzz
        PRIVATE
            "${PROJECT_SOURCE_DIR}/src"
    )

    target_link_libraries(memoptima_strict_json_fuzz
        PRIVATE
            memoptima_build_options
            nlohmann_json::nlohmann_json
    )

    target_compile_options(memoptima_strict_json_fuzz PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(memoptima_strict_json_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
    )

    set_target_properties(memoptima_strict_json_fuzz PROPERTIES
        CXX_EXTENSIONS OFF
    )

    add_executable(memoptima_evidence_record_fuzz
        "${PROJECT_SOURCE_DIR}/src/evidence_record_parser.cpp"
        "${PROJECT_SOURCE_DIR}/src/strict_json_parser.cpp"
        evidence_record_fuzz.cpp
        evidence_record_invariants.cpp
    )
    target_include_directories(memoptima_evidence_record_fuzz PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
    )
    target_link_libraries(memoptima_evidence_record_fuzz
        PRIVATE
            memoptima_build_options
            nlohmann_json::nlohmann_json
    )
    target_compile_options(memoptima_evidence_record_fuzz PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(memoptima_evidence_record_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
    )
    set_target_properties(memoptima_evidence_record_fuzz PROPERTIES
        CXX_EXTENSIONS OFF
    )

    add_executable(memoptima_terminal_text_fuzz
        "${PROJECT_SOURCE_DIR}/src/cli/formatting.cpp"
        terminal_text_fuzz.cpp
        terminal_text_invariants.cpp
    )
    target_include_directories(memoptima_terminal_text_fuzz PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
    )
    target_link_libraries(memoptima_terminal_text_fuzz
        PRIVATE
            memoptima_build_options
    )
    target_compile_options(memoptima_terminal_text_fuzz PRIVATE
        -fsanitize=fuzzer-no-link,address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(memoptima_terminal_text_fuzz PRIVATE
        -fsanitize=fuzzer,address,undefined
    )
    set_target_properties(memoptima_terminal_text_fuzz PROPERTIES
        CXX_EXTENSIONS OFF
    )
endif()
