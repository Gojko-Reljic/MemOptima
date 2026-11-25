cmake_minimum_required(VERSION 3.20)

project(MemOptima VERSION 0.9.1 LANGUAGES CXX)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(MEMOPTIMA_DOCKER_TEST_IMAGE "alpine:3.24.4" CACHE STRING
    "Docker image used by the explicitly invoked MemOptima integration check.")
find_program(MEMOPTIMA_DOCKER_EXECUTABLE NAMES docker)

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

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src)

# Docker is used only by this explicitly invoked integration target, never by the MemOptima binary.
add_custom_target(memoptima_docker_integration
    COMMAND "${CMAKE_COMMAND}"
        "-DMEMOPTIMA_EXECUTABLE=$<TARGET_FILE:memoptima>"
        "-DMEMOPTIMA_DOCKER_EXECUTABLE=${MEMOPTIMA_DOCKER_EXECUTABLE}"
        "-DMEMOPTIMA_DOCKER_TEST_IMAGE=${MEMOPTIMA_DOCKER_TEST_IMAGE}"
        -P "${PROJECT_SOURCE_DIR}/tests/docker_integration_check.cmake"
    COMMENT "Running the opt-in Docker cgroup v2 integration check"
    DEPENDS memoptima
    USES_TERMINAL
    VERBATIM
)

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
        "${PROJECT_SOURCE_DIR}/docs/0.2.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.3.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.4.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.5.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.6.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.7.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.8.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.9.0-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/0.9.1-scope.md"
        "${PROJECT_SOURCE_DIR}/docs/evidence-artifact-format.md"
        "${PROJECT_SOURCE_DIR}/docs/trend-manifest-format.md"
        "${PROJECT_SOURCE_DIR}/docs/trend-report-format.md"
        "${PROJECT_SOURCE_DIR}/docs/forecast-evaluation-report-format.md"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)

if(BUILD_TESTING)
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
