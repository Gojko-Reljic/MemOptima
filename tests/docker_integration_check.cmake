if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

if(NOT DEFINED MEMOPTIMA_DOCKER_EXECUTABLE OR
   "${MEMOPTIMA_DOCKER_EXECUTABLE}" MATCHES "-NOTFOUND$")
    message(FATAL_ERROR
        "Docker integration requires the Docker CLI. Install Docker or set MEMOPTIMA_DOCKER_EXECUTABLE.")
endif()

if(NOT DEFINED MEMOPTIMA_DOCKER_TEST_IMAGE)
    message(FATAL_ERROR "MEMOPTIMA_DOCKER_TEST_IMAGE must be provided.")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_DOCKER_EXECUTABLE}" info --format "{{json .SecurityOptions}}"
    RESULT_VARIABLE docker_info_result
    OUTPUT_VARIABLE docker_security_options
    ERROR_VARIABLE docker_info_error
)

if(NOT docker_info_result EQUAL 0)
    message(FATAL_ERROR
        "Docker integration requires a reachable Docker daemon: ${docker_info_error}")
endif()

if(docker_security_options MATCHES "name=rootless")
    message(FATAL_ERROR "Docker integration requires rootful Docker; rootless Docker is unsupported.")
endif()

string(RANDOM LENGTH 12 ALPHABET "abcdefghijklmnopqrstuvwxyz0123456789" test_suffix)
set(test_container_name "memoptima-integration-${test_suffix}")
set(test_container_id "")
set(test_artifact_path "${CMAKE_CURRENT_BINARY_DIR}/memoptima-integration-${test_suffix}.jsonl")

function(clean_up_test_artifact)
    if(EXISTS "${test_artifact_path}")
        file(REMOVE "${test_artifact_path}")
    endif()
endfunction()

function(clean_up_test_container)
    if(test_container_id STREQUAL "")
        return()
    endif()

    execute_process(
        COMMAND "${MEMOPTIMA_DOCKER_EXECUTABLE}" rm --force "${test_container_id}"
        RESULT_VARIABLE cleanup_result
        ERROR_VARIABLE cleanup_error
    )
    if(NOT cleanup_result EQUAL 0)
        message(FATAL_ERROR
            "Unable to remove MemOptima integration container ${test_container_id}: ${cleanup_error}")
    endif()
endfunction()

function(fail_after_cleanup message_text)
    clean_up_test_artifact()
    clean_up_test_container()
    message(FATAL_ERROR "${message_text}")
endfunction()

execute_process(
    COMMAND "${MEMOPTIMA_DOCKER_EXECUTABLE}" run --detach --name "${test_container_name}"
        --label "com.memoptima.integration=true" "${MEMOPTIMA_DOCKER_TEST_IMAGE}" sh -c
        "while true; do sleep 3600; done"
    RESULT_VARIABLE docker_run_result
    OUTPUT_VARIABLE test_container_id
    ERROR_VARIABLE docker_run_error
)

if(NOT docker_run_result EQUAL 0)
    message(FATAL_ERROR
        "Unable to start the MemOptima integration container from ${MEMOPTIMA_DOCKER_TEST_IMAGE}: ${docker_run_error}")
endif()

string(STRIP "${test_container_id}" test_container_id)
string(LENGTH "${test_container_id}" test_container_id_length)
if(NOT test_container_id_length EQUAL 64 OR NOT test_container_id MATCHES "^[0-9a-fA-F]+$")
    fail_after_cleanup("Docker returned an invalid container ID: ${test_container_id}")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" containers
    RESULT_VARIABLE containers_result
    OUTPUT_VARIABLE containers_output
    ERROR_VARIABLE containers_error
)

if(NOT containers_result EQUAL 0)
    fail_after_cleanup("memoptima containers failed: ${containers_error}")
endif()

string(FIND "${containers_output}" "${test_container_id}" container_position)
if(container_position EQUAL -1)
    fail_after_cleanup(
        "memoptima containers did not discover the test container ${test_container_id}. Error output: ${containers_error}")
endif()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" assess containers
    RESULT_VARIABLE assessments_result
    OUTPUT_VARIABLE assessments_output
    ERROR_VARIABLE assessments_error
)

if(NOT assessments_result EQUAL 0)
    fail_after_cleanup("memoptima assess containers failed: ${assessments_error}")
endif()

foreach(expected_text "Docker container assessments" "${test_container_id}")
    string(FIND "${assessments_output}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        fail_after_cleanup(
            "memoptima assess containers output is missing '${expected_text}': ${assessments_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" container "${test_container_id}"
    RESULT_VARIABLE container_result
    OUTPUT_VARIABLE container_output
    ERROR_VARIABLE container_error
)

if(NOT container_result EQUAL 0)
    fail_after_cleanup("memoptima container failed: ${container_error}")
endif()

foreach(expected_text "Container" "${test_container_id}" "Cgroup path:" "Memory pressure")
    string(FIND "${container_output}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        fail_after_cleanup(
            "memoptima container output is missing '${expected_text}': ${container_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" assess container "${test_container_id}"
    RESULT_VARIABLE assessment_result
    OUTPUT_VARIABLE assessment_output
    ERROR_VARIABLE assessment_error
)

if(NOT assessment_result EQUAL 0)
    fail_after_cleanup("memoptima assess container failed: ${assessment_error}")
endif()

foreach(expected_text "Container" "${test_container_id}" "Overall risk" "Coverage" "Findings")
    string(FIND "${assessment_output}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        fail_after_cleanup(
            "memoptima assess container output is missing '${expected_text}': ${assessment_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${MEMOPTIMA_EXECUTABLE}" sample containers "--count" 2 "--interval" 100ms
        "--output" "${test_artifact_path}"
    RESULT_VARIABLE sampling_result
    OUTPUT_VARIABLE sampling_output
    ERROR_VARIABLE sampling_error
)

if(NOT sampling_result EQUAL 0)
    fail_after_cleanup("memoptima sample containers failed: ${sampling_error}")
endif()

if(NOT sampling_output STREQUAL "")
    fail_after_cleanup("memoptima sample containers wrote to standard output: ${sampling_output}")
endif()

foreach(expected_text "Sampling session completed: 2 sample(s), artifact:"
                      "${test_artifact_path}")
    string(FIND "${sampling_error}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        fail_after_cleanup(
            "memoptima sample containers summary is missing '${expected_text}': ${sampling_error}")
    endif()
endforeach()

if(NOT EXISTS "${test_artifact_path}")
    fail_after_cleanup("memoptima sample containers did not create its artifact.")
endif()

file(READ "${test_artifact_path}" artifact_contents)
foreach(expected_text "\"record_type\":\"session_start\""
                      "\"record_type\":\"sample\""
                      "\"record_type\":\"session_end\""
                      "\"container_id\":\"${test_container_id}\"")
    string(FIND "${artifact_contents}" "${expected_text}" expected_position)
    if(expected_position EQUAL -1)
        fail_after_cleanup(
            "Sampling artifact is missing '${expected_text}': ${artifact_contents}")
    endif()
endforeach()

clean_up_test_artifact()

clean_up_test_container()
message(STATUS "MemOptima Docker integration check passed for ${test_container_id}.")
