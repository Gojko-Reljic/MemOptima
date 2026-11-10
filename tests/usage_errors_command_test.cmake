if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

function(assert_usage_error expected_error)
    execute_process(
        COMMAND "${MEMOPTIMA_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )

    if(NOT command_result EQUAL 2)
        message(FATAL_ERROR "Usage error returned exit code ${command_result}, expected 2.")
    endif()

    if(NOT command_output STREQUAL "")
        message(FATAL_ERROR "Usage error wrote to standard output: ${command_output}")
    endif()

    foreach(expected_text "${expected_error}" "Usage:")
        string(FIND "${command_error}" "${expected_text}" match_position)
        if(match_position EQUAL -1)
            message(FATAL_ERROR "Usage error output is missing: ${expected_text}")
        endif()
    endforeach()
endfunction()

assert_usage_error("Error: unknown option 'unknown'." unknown)
assert_usage_error("Error: expected at most one option." status unexpected)
