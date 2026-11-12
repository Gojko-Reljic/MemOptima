if(NOT DEFINED MEMOPTIMA_BUILD_DIRECTORY)
    message(FATAL_ERROR "MEMOPTIMA_BUILD_DIRECTORY must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_INSTALL_DIRECTORY)
    message(FATAL_ERROR "MEMOPTIMA_INSTALL_DIRECTORY must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_INSTALL_BINDIR)
    message(FATAL_ERROR "MEMOPTIMA_INSTALL_BINDIR must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_INSTALL_DOCDIR)
    message(FATAL_ERROR "MEMOPTIMA_INSTALL_DOCDIR must be provided.")
endif()

# This controlled path is always <build>/install-test, so each test run starts with a clean prefix.
file(REMOVE_RECURSE "${MEMOPTIMA_INSTALL_DIRECTORY}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" --install "${MEMOPTIMA_BUILD_DIRECTORY}" --prefix "${MEMOPTIMA_INSTALL_DIRECTORY}"
    RESULT_VARIABLE install_result
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)

if(NOT install_result EQUAL 0)
    message(FATAL_ERROR "MemOptima installation failed with exit code ${install_result}: ${install_error}")
endif()

set(installed_executable "${MEMOPTIMA_INSTALL_DIRECTORY}/${MEMOPTIMA_INSTALL_BINDIR}/memoptima")
set(installed_document_directory "${MEMOPTIMA_INSTALL_DIRECTORY}/${MEMOPTIMA_INSTALL_DOCDIR}")

foreach(required_file
    "${installed_executable}"
    "${installed_document_directory}/LICENSE"
    "${installed_document_directory}/README.md"
    "${installed_document_directory}/user-guide.md"
    "${installed_document_directory}/metrics-and-pressure.md"
    "${installed_document_directory}/mvp-scope.md"
    "${installed_document_directory}/0.2.0-scope.md"
    "${installed_document_directory}/0.3.0-scope.md"
    "${installed_document_directory}/0.4.0-scope.md"
    "${installed_document_directory}/evidence-artifact-format.md")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Expected installed file is missing: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${installed_executable}" --version
    RESULT_VARIABLE version_result
    OUTPUT_VARIABLE version_output
    ERROR_VARIABLE version_error
)

if(NOT version_result EQUAL 0)
    message(FATAL_ERROR "Installed memoptima --version failed with exit code ${version_result}: ${version_error}")
endif()

if(NOT version_error STREQUAL "")
    message(FATAL_ERROR "Installed memoptima --version wrote to standard error: ${version_error}")
endif()

string(FIND "${version_output}" "memoptima " version_prefix_position)
if(NOT version_prefix_position EQUAL 0)
    message(FATAL_ERROR "Installed memoptima --version output must begin with 'memoptima ': ${version_output}")
endif()
