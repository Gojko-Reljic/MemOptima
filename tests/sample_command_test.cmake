if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef artifact_suffix)
set(artifact_path "${CMAKE_CURRENT_BINARY_DIR}/memoptima-sample-${artifact_suffix}.jsonl")

function(assert_usage_error expected_error)
    execute_process(
        COMMAND "${MEMOPTIMA_EXECUTABLE}" sample containers ${ARGN}
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
    string(FIND "${command_error}" "${expected_error}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "Usage error output is missing: ${expected_error}")
    endif()
    if(EXISTS "${artifact_path}")
        message(FATAL_ERROR "Usage error unexpectedly created an artifact.")
    endif()
endfunction()

assert_usage_error("requires --count, --interval, and --output" --count 1 --interval 100ms)
assert_usage_error("invalid --count" --count 0 --interval 100ms --output "${artifact_path}")
assert_usage_error("invalid --interval" --count 1 --interval 99ms --output "${artifact_path}")
assert_usage_error("received unknown option '--unknown'" --count 1 --interval 100ms --unknown value)
assert_usage_error("received --count more than once" --count 1 --count 2 --output "${artifact_path}")

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" sample containers --output "${artifact_path}" --interval 100ms --count 1
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error
)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "Sampling command failed with exit code ${command_result}: ${command_error}")
endif()
if(NOT command_output STREQUAL "")
    message(FATAL_ERROR "Sampling command wrote to standard output: ${command_output}")
endif()
string(FIND "${command_error}" "Sampling session completed: 1 sample(s), artifact:" summary_position)
if(summary_position EQUAL -1)
    message(FATAL_ERROR "Sampling command summary is missing or unclear: ${command_error}")
endif()
if(NOT EXISTS "${artifact_path}")
    message(FATAL_ERROR "Sampling command did not create its artifact.")
endif()

file(READ "${artifact_path}" artifact_contents)
string(FIND "${artifact_contents}" "\"record_type\":\"session_start\"" start_position)
string(FIND "${artifact_contents}" "\"record_type\":\"sample\"" sample_position)
string(FIND "${artifact_contents}" "\"record_type\":\"session_end\"" end_position)
if(start_position EQUAL -1 OR sample_position EQUAL -1 OR end_position EQUAL -1)
    message(FATAL_ERROR "Sampling artifact is missing required JSON Lines records.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" inspect evidence "${artifact_path}"
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_output
    ERROR_VARIABLE inspect_error
)
if(NOT inspect_result EQUAL 0 OR NOT inspect_error STREQUAL "")
    message(FATAL_ERROR "Evidence inspection failed: ${inspect_error}")
endif()
string(FIND "${inspect_output}" "Completed samples: 1" inspect_position)
if(inspect_position EQUAL -1)
    message(FATAL_ERROR "Evidence inspection output is missing its sample summary: ${inspect_output}")
endif()

file(REMOVE "${artifact_path}")
