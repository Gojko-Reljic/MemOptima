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
endif()
