add_library(memoptima_core STATIC
    application_info.cpp
    analysis/memory_pressure.cpp
    analysis/memory_status.cpp
    linux/meminfo_parser.cpp
    linux/memory_reader.cpp
    linux/process_ranker.cpp
    linux/process_scanner.cpp
    linux/process_status_parser.cpp
    memory_snapshot.cpp
    process_memory_info.cpp
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
)

set_target_properties(memoptima_core PROPERTIES
    CXX_EXTENSIONS OFF
)

add_library(memoptima_cli_support STATIC
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
