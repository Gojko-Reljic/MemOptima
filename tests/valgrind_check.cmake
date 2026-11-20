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
run_valgrind_command(assess-containers 0 assess containers)
run_valgrind_command(assess-container-unknown-id 1 assess container
    aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa)

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef sample_artifact_suffix)
set(sample_artifact_path "${CMAKE_CURRENT_BINARY_DIR}/memoptima-valgrind-${sample_artifact_suffix}.jsonl")
run_valgrind_command(sample-containers 0 sample containers "--count" 1 "--interval" 100ms
    "--output" "${sample_artifact_path}")
if(NOT EXISTS "${sample_artifact_path}")
    message(FATAL_ERROR "Valgrind sampling check did not create its artifact.")
endif()
run_valgrind_command(inspect-evidence 0 inspect evidence "${sample_artifact_path}")
run_valgrind_command(forecast-evidence 0 forecast evidence "${sample_artifact_path}")
file(REMOVE "${sample_artifact_path}")

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef trend_artifact_suffix)
set(trend_directory "${CMAKE_CURRENT_BINARY_DIR}/memoptima-valgrind-trends-${trend_artifact_suffix}")
file(MAKE_DIRECTORY "${trend_directory}")
set(trend_artifact_path "${trend_directory}/evidence.jsonl")
set(trend_manifest_path "${trend_directory}/manifest.json")
# A non-empty fixture exercises trend analysis and table formatting, not just manifest handling.
file(WRITE "${trend_artifact_path}"
    "{\"record_type\":\"session_start\",\"schema_version\":1,\"application_version\":\"0.7.0\",\"started_at_utc\":\"2026-01-01T00:00:00.000Z\",\"requested_sample_count\":3,\"requested_interval_ms\":\"10000\",\"output_max_bytes\":\"536870912\",\"output_permissions_octal\":\"0600\",\"flush_after_each_record\":true,\"host_identifiers_included\":false}\n"
    "{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":1,\"planned_offset_ms\":\"0\",\"captured_at_utc\":\"2026-01-01T00:00:00.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[{\"container_id\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"cgroup_path\":\"/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"memory\":{\"current_bytes\":\"100\",\"high_bytes\":\"max\",\"max_bytes\":\"400\",\"events\":{\"high\":\"0\",\"max\":\"0\",\"oom\":\"0\",\"oom_kill\":\"0\"},\"stat\":{\"anonymous_bytes\":\"0\",\"file_backed_bytes\":\"0\"},\"swap_current_bytes\":\"0\",\"psi\":{\"some\":null,\"full\":null},\"unavailable_metrics\":[]},\"assessment\":{\"overall_risk\":\"low\",\"coverage\":\"complete\",\"findings\":[],\"unavailable_evidence\":[]}}]}\n"
    "{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":2,\"planned_offset_ms\":\"10000\",\"captured_at_utc\":\"2026-01-01T00:00:10.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[{\"container_id\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"cgroup_path\":\"/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"memory\":{\"current_bytes\":\"200\",\"high_bytes\":\"max\",\"max_bytes\":\"400\",\"events\":{\"high\":\"0\",\"max\":\"0\",\"oom\":\"0\",\"oom_kill\":\"0\"},\"stat\":{\"anonymous_bytes\":\"0\",\"file_backed_bytes\":\"0\"},\"swap_current_bytes\":\"0\",\"psi\":{\"some\":null,\"full\":null},\"unavailable_metrics\":[]},\"assessment\":{\"overall_risk\":\"low\",\"coverage\":\"complete\",\"findings\":[],\"unavailable_evidence\":[]}}]}\n"
    "{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":3,\"planned_offset_ms\":\"20000\",\"captured_at_utc\":\"2026-01-01T00:00:20.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[{\"container_id\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"cgroup_path\":\"/docker/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"memory\":{\"current_bytes\":\"300\",\"high_bytes\":\"max\",\"max_bytes\":\"400\",\"events\":{\"high\":\"0\",\"max\":\"0\",\"oom\":\"0\",\"oom_kill\":\"0\"},\"stat\":{\"anonymous_bytes\":\"0\",\"file_backed_bytes\":\"0\"},\"swap_current_bytes\":\"0\",\"psi\":{\"some\":null,\"full\":null},\"unavailable_metrics\":[]},\"assessment\":{\"overall_risk\":\"low\",\"coverage\":\"complete\",\"findings\":[],\"unavailable_evidence\":[]}}]}\n"
    "{\"record_type\":\"session_end\",\"schema_version\":1,\"outcome\":\"completed\",\"completed_sample_count\":3,\"ended_at_utc\":\"2026-01-01T00:00:20.001Z\",\"failure\":null}\n")
file(WRITE "${trend_manifest_path}" "{\"manifest_version\":1,\"artifacts\":[{\"path\":\"evidence.jsonl\"}]}")
run_valgrind_command(trends-analyze 0 trends analyze "${trend_manifest_path}")
file(REMOVE_RECURSE "${trend_directory}")

execute_process(
    COMMAND "${MEMOPTIMA_TIMEOUT_EXECUTABLE}" --preserve-status -s INT 2
        "${MEMOPTIMA_VALGRIND_EXECUTABLE}" ${MEMOPTIMA_VALGRIND_OPTIONS}
        "${MEMOPTIMA_EXECUTABLE}" monitor
    RESULT_VARIABLE monitor_result
)

if(NOT monitor_result EQUAL 0)
    message(FATAL_ERROR "Valgrind check failed for memoptima monitor with exit code ${monitor_result}.")
endif()
