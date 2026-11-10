if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" status
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "memoptima status failed with exit code ${command_result}: ${command_error}")
endif()

foreach(expected_text "Memory status" "RAM" "Swap" "In use (estimate):" "Memory pressure" "Level:" "Recommendation:")
    string(FIND "${command_output}" "${expected_text}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "memoptima status output is missing: ${expected_text}")
    endif()
endforeach()
