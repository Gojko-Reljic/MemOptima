cmake_minimum_required(VERSION 3.20)

project(MemOptima VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
include(GNUInstallDirs)

option(MEMOPTIMA_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer." OFF)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    message(FATAL_ERROR "MemOptima currently supports GCC and Clang only.")
endif()

# Centralize the language standard and strict warnings so every project target uses the same policy.
add_library(memoptima_build_options INTERFACE)

target_compile_features(memoptima_build_options INTERFACE cxx_std_23)
target_compile_options(memoptima_build_options INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Werror
)

# Sanitizers are opt-in so Release builds remain suitable for normal distribution and Valgrind.
if(MEMOPTIMA_ENABLE_SANITIZERS)
    target_compile_options(memoptima_build_options INTERFACE
        -fsanitize=address,undefined
        -fno-omit-frame-pointer
    )
    target_link_options(memoptima_build_options INTERFACE
        -fsanitize=address,undefined
    )
endif()

# The project version is the single source of truth; this header is generated in the build tree.
configure_file(
    cmake/version.hpp.in
    "${CMAKE_CURRENT_BINARY_DIR}/generated/include/memoptima/version.hpp"
    @ONLY
)

add_subdirectory(src)

# Formatting is a development quality check, not a dependency of builds that explicitly disable tests.
if(BUILD_TESTING)
    find_program(MEMOPTIMA_CLANG_FORMAT_EXECUTABLE NAMES clang-format REQUIRED)

    file(GLOB_RECURSE MEMOPTIMA_FORMAT_SOURCES CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/*.hpp"
        "${PROJECT_SOURCE_DIR}/src/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/*.cpp"
        "${PROJECT_SOURCE_DIR}/tests/*.hpp"
    )

    add_custom_target(memoptima_format_check
        COMMAND "${MEMOPTIMA_CLANG_FORMAT_EXECUTABLE}" --dry-run --Werror ${MEMOPTIMA_FORMAT_SOURCES}
        COMMENT "Checking C++ formatting with clang-format"
        VERBATIM
    )

    add_custom_target(memoptima_format
        COMMAND "${MEMOPTIMA_CLANG_FORMAT_EXECUTABLE}" -i ${MEMOPTIMA_FORMAT_SOURCES}
        COMMENT "Formatting C++ sources with clang-format"
        VERBATIM
    )
endif()

# GNUInstallDirs keeps the executable and documentation in standard, prefix-configurable locations.
install(
    TARGETS memoptima
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

install(
    FILES
        "${PROJECT_SOURCE_DIR}/LICENSE"
        "${PROJECT_SOURCE_DIR}/README.md"
        "${PROJECT_SOURCE_DIR}/docs/user-guide.md"
        "${PROJECT_SOURCE_DIR}/docs/metrics-and-pressure.md"
        "${PROJECT_SOURCE_DIR}/docs/mvp-scope.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

if(BUILD_TESTING)
    include(FetchContent)

# Pin GoogleTest to a commit so test builds remain reproducible over time.
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 063de7e9578f82b369302001269680b4b1553359
    )
    FetchContent_MakeAvailable(googletest)

    add_subdirectory(tests)

    add_test(
        NAME MemOptimaFormat
        COMMAND "${CMAKE_COMMAND}" --build "${PROJECT_BINARY_DIR}" --target memoptima_format_check
    )
endif()

# Run Valgrind only for Release: Debug already uses sanitizers, and combining both is not useful.
if(BUILD_TESTING AND CMAKE_BUILD_TYPE STREQUAL "Release")
    find_program(MEMOPTIMA_VALGRIND_EXECUTABLE NAMES valgrind REQUIRED)
    find_program(MEMOPTIMA_TIMEOUT_EXECUTABLE NAMES timeout REQUIRED)

    add_custom_target(memoptima_valgrind
        COMMAND "${CMAKE_COMMAND}"
            "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
            "-DMEMOPTIMA_VALGRIND_EXECUTABLE=${MEMOPTIMA_VALGRIND_EXECUTABLE}"
            "-DMEMOPTIMA_TIMEOUT_EXECUTABLE=${MEMOPTIMA_TIMEOUT_EXECUTABLE}"
            -P "${PROJECT_SOURCE_DIR}/tests/valgrind_check.cmake"
        COMMENT "Running Release CLI checks under Valgrind"
        DEPENDS memoptima
        USES_TERMINAL
        VERBATIM
    )
endif()
