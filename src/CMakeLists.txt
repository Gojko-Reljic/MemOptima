add_library(memoptima_core STATIC
    application_info.cpp
)

target_include_directories(memoptima_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
    PRIVATE
        "${PROJECT_BINARY_DIR}/generated/include"
)

target_link_libraries(memoptima_core
    PRIVATE
        memoptima_build_options
)

set_target_properties(memoptima_core PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(memoptima
    main.cpp
)

target_link_libraries(memoptima
    PRIVATE
        memoptima_build_options
        memoptima_core
)

set_target_properties(memoptima PROPERTIES
    CXX_EXTENSIONS OFF
)
