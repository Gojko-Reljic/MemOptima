if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" processes
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "memoptima processes failed with exit code ${command_result}: ${command_error}")
endif()

if(NOT command_error STREQUAL "")
    message(FATAL_ERROR "memoptima processes wrote to standard error: ${command_error}")
endif()

string(FIND "${command_output}" "Top memory processes (VmRSS)" table_position)
string(FIND "${command_output}" "No readable process memory records were found." empty_position)
if(table_position EQUAL -1 AND empty_position EQUAL -1)
    message(FATAL_ERROR "memoptima processes output does not describe process results.")
endif()
