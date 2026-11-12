if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" assess containers
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "memoptima assess containers failed with exit code ${command_result}: ${command_error}")
endif()

string(FIND "${command_output}" "Docker container assessments" table_position)
string(FIND "${command_output}" "No supported rootful Docker containers found." empty_position)
if(table_position EQUAL -1 AND empty_position EQUAL -1)
    message(FATAL_ERROR "memoptima assess containers output does not describe assessment results.")
endif()

set(unknown_container_id "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" assess container "${unknown_container_id}"
    RESULT_VARIABLE detail_result
    OUTPUT_VARIABLE detail_output
    ERROR_VARIABLE detail_error
)

if(NOT detail_result EQUAL 1)
    message(FATAL_ERROR "Unknown assessed Docker container ID returned exit code ${detail_result}, expected 1.")
endif()

if(NOT detail_output STREQUAL "")
    message(FATAL_ERROR "Unknown assessed Docker container ID wrote to standard output: ${detail_output}")
endif()

string(FIND "${detail_error}" "Error: Docker container '${unknown_container_id}' was not found" error_position)
if(error_position EQUAL -1)
    message(FATAL_ERROR "Unknown assessed Docker container ID error is missing or unclear: ${detail_error}")
endif()
