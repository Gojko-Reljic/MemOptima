add_executable(memoptima_core_tests
    application_info_test.cpp
    analysis/cgroup_memory_assessment_test.cpp
    analysis/memory_pressure_test.cpp
    analysis/memory_status_test.cpp
    cgroup_memory_snapshot_test.cpp
    cli/assessments_formatter_test.cpp
    docker_container_assessment_test.cpp
    docker_container_memory_snapshot_test.cpp
    evidence_input_reader_test.cpp
    cli/containers_formatter_test.cpp
    cli/monitor_runner_test.cpp
    cli/processes_formatter_test.cpp
    cli/status_formatter_test.cpp
    linux/cgroup_v2/mountinfo_parser_test.cpp
    linux/cgroup_v2/cgroup_memory_parser_test.cpp
    linux/cgroup_v2/cgroup_memory_reader_test.cpp
    linux/cgroup_v2/docker_container_scanner_test.cpp
    linux/cgroup_v2/psi_parser_test.cpp
    linux/process_ranker_test.cpp
    linux/process_status_parser_test.cpp
    memory_snapshot_test.cpp
    linux/meminfo_parser_test.cpp
    process_memory_info_test.cpp
    sampling/evidence_models_test.cpp
    sampling/jsonl_serializer_test.cpp
    sampling/jsonl_writer_test.cpp
    sampling/sampling_configuration_test.cpp
    sampling/session_runner_test.cpp
    utc_timestamp_test.cpp
)

target_include_directories(memoptima_core_tests
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(memoptima_core_tests
    PRIVATE
        memoptima_build_options
        memoptima_core
        memoptima_cli_support
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

set_target_properties(memoptima_core_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

include(GoogleTest)
gtest_discover_tests(memoptima_core_tests DISCOVERY_MODE PRE_TEST)

find_program(MEMOPTIMA_TIMEOUT_EXECUTABLE timeout REQUIRED)

add_test(
    NAME MemOptimaCli.Help
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/help_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Version
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/version_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.UsageErrors
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/usage_errors_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Status
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/status_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Processes
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/processes_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Containers
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/containers_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Assess
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/assess_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Monitor
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        "-DMEMOPTIMA_TIMEOUT_EXECUTABLE=${MEMOPTIMA_TIMEOUT_EXECUTABLE}"
        -P "${PROJECT_SOURCE_DIR}/tests/monitor_command_test.cmake"
)

add_test(
    NAME MemOptimaCli.Sample
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        -P "${PROJECT_SOURCE_DIR}/tests/sample_command_test.cmake"
)

add_test(
    NAME MemOptimaInstall
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_BUILD_DIRECTORY=${PROJECT_BINARY_DIR}"
        "-DMEMOPTIMA_INSTALL_DIRECTORY=${PROJECT_BINARY_DIR}/install-test"
        "-DMEMOPTIMA_INSTALL_BINDIR=${CMAKE_INSTALL_BINDIR}"
        "-DMEMOPTIMA_INSTALL_DOCDIR=${CMAKE_INSTALL_DOCDIR}"
        -P "${PROJECT_SOURCE_DIR}/tests/install_test.cmake"
)
