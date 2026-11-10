if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

function(assert_version_command)
    execute_process(
        COMMAND "${MEMOPTIMA_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
        OUTPUT_VARIABLE command_output
        ERROR_VARIABLE command_error
    )

    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR "Version command failed with exit code ${command_result}: ${command_error}")
    endif()

    if(NOT command_error STREQUAL "")
        message(FATAL_ERROR "Version command wrote to standard error: ${command_error}")
    endif()

    string(FIND "${command_output}" "memoptima " match_position)
    if(NOT match_position EQUAL 0)
        message(FATAL_ERROR "Version output must begin with 'memoptima ': ${command_output}")
    endif()
endfunction()

assert_version_command(-v)
assert_version_command(--version)
