# Keep collection and analysis reusable; terminal-specific presentation belongs to cli_support.
add_library(memoptima_core STATIC
    application_info.cpp
    analysis/cgroup_memory_assessment.cpp
    analysis/memory_pressure.cpp
    analysis/memory_status.cpp
    cgroup_memory_snapshot.cpp
    docker_container_assessment.cpp
    docker_container_memory_snapshot.cpp
    sampling/evidence_models.cpp
    sampling/jsonl_serializer.cpp
    sampling/jsonl_writer.cpp
    linux/cgroup_v2/cgroup_v2_capability_checker.cpp
    linux/cgroup_v2/cgroup_memory_parser.cpp
    linux/cgroup_v2/cgroup_memory_reader.cpp
    linux/cgroup_v2/docker_container_scanner.cpp
    linux/cgroup_v2/mountinfo_parser.cpp
    linux/cgroup_v2/psi_parser.cpp
    linux/meminfo_parser.cpp
    linux/memory_reader.cpp
    linux/process_ranker.cpp
    linux/process_scanner.cpp
    linux/process_status_parser.cpp
    memory_snapshot.cpp
    process_memory_info.cpp
    sampling/sampling_configuration.cpp
    utc_timestamp.cpp
)

target_include_directories(memoptima_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
        "${PROJECT_BINARY_DIR}/generated/include"
)

target_link_libraries(memoptima_core
    PRIVATE
        memoptima_build_options
        nlohmann_json::nlohmann_json
)

set_target_properties(memoptima_core PROPERTIES
    CXX_EXTENSIONS OFF
)

add_library(memoptima_cli_support STATIC
    cli/assessments_formatter.cpp
    cli/containers_formatter.cpp
    cli/monitor_runner.cpp
    cli/processes_formatter.cpp
    cli/status_formatter.cpp
)

target_include_directories(memoptima_cli_support
    PRIVATE
        "${PROJECT_SOURCE_DIR}/include"
        "${PROJECT_SOURCE_DIR}/src"
)

target_link_libraries(memoptima_cli_support
    PRIVATE
        memoptima_build_options
        memoptima_core
)

set_target_properties(memoptima_cli_support PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(memoptima
    main.cpp
)

target_link_libraries(memoptima
    PRIVATE
        memoptima_build_options
        memoptima_core
        memoptima_cli_support
)

set_target_properties(memoptima PROPERTIES
    CXX_EXTENSIONS OFF
)
