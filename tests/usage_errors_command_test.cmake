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
assert_usage_error("Error: 'container' requires a full Docker container ID." container)
assert_usage_error("Error: invalid Docker container ID:" container invalid)
assert_usage_error("Error: 'assess' requires 'containers' or 'container <FULL_ID>'." assess)
assert_usage_error("Error: invalid Docker container ID:" assess container invalid)
assert_usage_error("Error: 'inspect' requires 'evidence <FILE>'." inspect)
assert_usage_error("Error: 'trends' requires 'analyze <MANIFEST>'." trends)
