if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

function(assert_help_command)
    execute_process(
        COMMAND "${MEMOPTIMA_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )

    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR "Help command failed with exit code ${command_result}: ${command_error}")
    endif()

    if(NOT command_error STREQUAL "")
        message(FATAL_ERROR "Help command wrote to standard error: ${command_error}")
    endif()

    foreach(expected_text "Usage:" "forecast evaluate <MANIFEST>" "monitor" "processes" "status" "trends")
        string(FIND "${command_output}" "${expected_text}" match_position)
        if(match_position EQUAL -1)
            message(FATAL_ERROR "Help output is missing: ${expected_text}")
        endif()
    endforeach()
endfunction()

assert_help_command()
assert_help_command(-h)
assert_help_command(--help)
