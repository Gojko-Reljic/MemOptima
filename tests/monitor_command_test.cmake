if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_TIMEOUT_EXECUTABLE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_TIMEOUT_EXECUTABLE}" --preserve-status -s INT 1 "${MEMOPTIMA_EXECUTABLE}" monitor
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "memoptima monitor failed with exit code ${command_result}: ${command_error}")
endif()

if(NOT command_error STREQUAL "")
    message(FATAL_ERROR "memoptima monitor wrote to standard error: ${command_error}")
endif()

string(REGEX MATCH "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z" timestamp "${command_output}")
if(timestamp STREQUAL "")
    message(FATAL_ERROR "memoptima monitor output is missing a UTC ISO 8601 timestamp: ${command_output}")
endif()

foreach(expected_text "Memory status" "Memory pressure" "Recommendation:")
    string(FIND "${command_output}" "${expected_text}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "memoptima monitor output is missing: ${expected_text}")
    endif()
endforeach()
