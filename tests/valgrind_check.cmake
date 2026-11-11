if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_VALGRIND_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_VALGRIND_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_TIMEOUT_EXECUTABLE must be provided.")
endif()

# Fail for memory errors and definite or indirect leaks, but do not fail only for still-reachable memory.
set(MEMOPTIMA_VALGRIND_OPTIONS
    --leak-check=full
    --show-leak-kinds=definite,indirect
    --errors-for-leak-kinds=definite,indirect
    --error-exitcode=1
)

function(run_valgrind_command command_name expected_exit_code)
    execute_process(
        COMMAND "${MEMOPTIMA_VALGRIND_EXECUTABLE}" ${MEMOPTIMA_VALGRIND_OPTIONS}
            "${MEMOPTIMA_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
    )

    if(NOT command_result EQUAL expected_exit_code)
        message(FATAL_ERROR
            "Valgrind check failed for memoptima ${command_name} with exit code ${command_result}, expected ${expected_exit_code}.")
    endif()
endfunction()

run_valgrind_command(status 0 status)
run_valgrind_command(processes 0 processes)
run_valgrind_command(containers 0 containers)
run_valgrind_command(container-invalid-id 2 container invalid)

execute_process(
    COMMAND "${MEMOPTIMA_TIMEOUT_EXECUTABLE}" --preserve-status -s INT 2
        "${MEMOPTIMA_VALGRIND_EXECUTABLE}" ${MEMOPTIMA_VALGRIND_OPTIONS}
        "${MEMOPTIMA_EXECUTABLE}" monitor
    RESULT_VARIABLE monitor_result
)

if(NOT monitor_result EQUAL 0)
    message(FATAL_ERROR "Valgrind check failed for memoptima monitor with exit code ${monitor_result}.")
endif()
