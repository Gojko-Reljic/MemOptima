if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef artifact_suffix)
set(test_directory "${CMAKE_CURRENT_BINARY_DIR}/memoptima-terminal-escape-${artifact_suffix}")
file(MAKE_DIRECTORY "${test_directory}")
set(artifact_path "${test_directory}/evidence.jsonl")
set(manifest_path "${test_directory}/manifest.json")

file(WRITE "${artifact_path}" [=[
{"record_type":"session_start","schema_version":1,"application_version":"0.9.0","started_at_utc":"2026-01-01T00:00:00.000Z","requested_sample_count":4,"requested_interval_ms":"10000","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false}
{"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-01-01T00:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m","memory":{"current_bytes":"100","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":2,"planned_offset_ms":"10000","captured_at_utc":"2026-01-01T00:00:10.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m","memory":{"current_bytes":"200","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":3,"planned_offset_ms":"20000","captured_at_utc":"2026-01-01T00:00:20.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m","memory":{"current_bytes":"300","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":4,"planned_offset_ms":"30000","captured_at_utc":"2026-01-01T00:00:30.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m","memory":{"current_bytes":"400","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":4,"ended_at_utc":"2026-01-01T00:00:30.001Z","failure":null}
]=])
file(WRITE "${manifest_path}" "{\"manifest_version\":1,\"artifacts\":[{\"path\":\"evidence.jsonl\"}]}")

set(expected_path [=[/docker/control\npath\x1B[31m]=])
set(raw_path "/docker/control\npath")
string(ASCII 27 escape_character)

function(assert_safe_path_output command_name output)
    string(FIND "${output}" "${expected_path}" escaped_path_position)
    if(escaped_path_position EQUAL -1)
        message(FATAL_ERROR "${command_name} did not render the escaped cgroup path: ${output}")
    endif()

    string(FIND "${output}" "${raw_path}" raw_path_position)
    if(NOT raw_path_position EQUAL -1)
        message(FATAL_ERROR "${command_name} rendered a cgroup path with a physical newline.")
    endif()

    string(FIND "${output}" "${escape_character}" escape_position)
    if(NOT escape_position EQUAL -1)
        message(FATAL_ERROR "${command_name} rendered an ESC control byte.")
    endif()
endfunction()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" trends analyze "${manifest_path}"
    RESULT_VARIABLE trends_result OUTPUT_VARIABLE trends_output ERROR_VARIABLE trends_error)
if(NOT trends_result EQUAL 0 OR NOT trends_error STREQUAL "")
    message(FATAL_ERROR "Trends command failed: ${trends_error}")
endif()
assert_safe_path_output("Trends command" "${trends_output}")

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast evaluate "${manifest_path}"
    RESULT_VARIABLE forecast_result OUTPUT_VARIABLE forecast_output ERROR_VARIABLE forecast_error)
if(NOT forecast_result EQUAL 0 OR NOT forecast_error STREQUAL "")
    message(FATAL_ERROR "Forecast evaluation command failed: ${forecast_error}")
endif()
assert_safe_path_output("Forecast evaluation command" "${forecast_output}")

file(REMOVE_RECURSE "${test_directory}")
