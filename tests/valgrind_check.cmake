if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_VALGRIND_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_VALGRIND_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_TIMEOUT_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_TIMEOUT_EXECUTABLE must be provided.")
endif()

set(MEMOPTIMA_VALGRIND_OPTIONS
    --leak-check=full
    --show-leak-kinds=definite,indirect
    --errors-for-leak-kinds=definite,indirect
    --error-exitcode=1
)

function(run_valgrind_command command_name)
    execute_process(
        COMMAND "${MEMOPTIMA_VALGRIND_EXECUTABLE}" ${MEMOPTIMA_VALGRIND_OPTIONS}
            "${MEMOPTIMA_EXECUTABLE}" ${ARGN}
        RESULT_VARIABLE command_result
    )

    if(NOT command_result EQUAL 0)
        message(FATAL_ERROR "Valgrind check failed for memoptima ${command_name} with exit code ${command_result}.")
    endif()
endfunction()

run_valgrind_command(status status)
run_valgrind_command(processes processes)

execute_process(
    COMMAND "${MEMOPTIMA_TIMEOUT_EXECUTABLE}" --preserve-status -s INT 2
        "${MEMOPTIMA_VALGRIND_EXECUTABLE}" ${MEMOPTIMA_VALGRIND_OPTIONS}
        "${MEMOPTIMA_EXECUTABLE}" monitor
    RESULT_VARIABLE monitor_result
)

if(NOT monitor_result EQUAL 0)
    message(FATAL_ERROR "Valgrind check failed for memoptima monitor with exit code ${monitor_result}.")
endif()
