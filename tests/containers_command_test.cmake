if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" containers
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "memoptima containers failed with exit code ${command_result}: ${command_error}")
endif()

string(FIND "${command_output}" "Docker container memory" table_position)
string(FIND "${command_output}" "No supported rootful Docker containers found." empty_position)
if(table_position EQUAL -1 AND empty_position EQUAL -1)
    message(FATAL_ERROR "memoptima containers output does not describe Docker container results.")
endif()
