if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef artifact_suffix)
set(test_directory "${CMAKE_CURRENT_BINARY_DIR}/memoptima-terminal-escape-${artifact_suffix}")
file(MAKE_DIRECTORY "${test_directory}")
string(ASCII 27 escape_character)
string(ASCII 194 155 c1_control)
string(ASCII 226 128 174 right_to_left_override)
string(ASCII 226 128 139 zero_width_space)
set(artifact_path "${test_directory}/evidence${escape_character}.jsonl")
set(poisoned_artifact_path "${test_directory}/poisoned.jsonl")
set(manifest_path "${test_directory}/manifest.json")
set(poisoned_manifest_path "${test_directory}/poisoned-manifest.json")

file(WRITE "${artifact_path}" [=[
{"record_type":"session_start","schema_version":1,"application_version":"0.9.0","started_at_utc":"2026-01-01T00:00:00.000Z","requested_sample_count":4,"requested_interval_ms":"10000","output_max_bytes":"536870912","output_permissions_octal":"0600","flush_after_each_record":true,"host_identifiers_included":false}
{"record_type":"sample","schema_version":1,"sample_sequence":1,"planned_offset_ms":"0","captured_at_utc":"2026-01-01T00:00:00.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m\u009b\u202e\u200b","memory":{"current_bytes":"100","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":2,"planned_offset_ms":"10000","captured_at_utc":"2026-01-01T00:00:10.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m\u009b\u202e\u200b","memory":{"current_bytes":"200","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":3,"planned_offset_ms":"20000","captured_at_utc":"2026-01-01T00:00:20.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m\u009b\u202e\u200b","memory":{"current_bytes":"300","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"sample","schema_version":1,"sample_sequence":4,"planned_offset_ms":"30000","captured_at_utc":"2026-01-01T00:00:30.000Z","schedule_delay_ms":"0","skipped_container_count":"0","discovery_warnings":[],"containers":[{"container_id":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","cgroup_path":"/docker/control\npath\u001b[31m\u009b\u202e\u200b","memory":{"current_bytes":"400","high_bytes":"max","max_bytes":"800","events":{"high":"0","max":"0","oom":"0","oom_kill":"0"},"stat":{"anonymous_bytes":"0","file_backed_bytes":"0"},"swap_current_bytes":"0","psi":{"some":null,"full":null},"unavailable_metrics":[]},"assessment":{"overall_risk":"low","coverage":"complete","findings":[],"unavailable_evidence":[]}}]}
{"record_type":"session_end","schema_version":1,"outcome":"completed","completed_sample_count":4,"ended_at_utc":"2026-01-01T00:00:30.001Z","failure":null}
]=])
file(WRITE "${manifest_path}" "{\"manifest_version\":1,\"artifacts\":[{\"path\":\"evidence\\u001b.jsonl\"}]}")
file(WRITE "${poisoned_artifact_path}" [=[{"record_type":"sample","schema_version":1,"\u001b\u202e\u200b":true,"\u001b\u202e\u200b":false}
]=])
file(WRITE "${poisoned_manifest_path}" "{\"manifest_version\":1,\"artifacts\":[{\"path\":\"poisoned.jsonl\"}],\"\\u001b\\u202e\":true,\"\\u001b\\u202e\":false}")

set(expected_path [=[/docker/control\npath\x1B[31m\u{9B}\u{202E}\u{200B}]=])
set(expected_duplicate_path [=[$["\\u001B\u{202E}\u{200B}"]]=])
set(expected_manifest_duplicate_path [=[$["\\u001B\u{202E}"]]=])
set(raw_path "/docker/control\npath")

function(assert_safe_path_output command_name output)
    string(FIND "${output}" "${expected_path}" escaped_path_position)
    if(escaped_path_position EQUAL -1)
        message(FATAL_ERROR "${command_name} did not render the escaped cgroup path: ${output}")
    endif()

    string(FIND "${output}" "${raw_path}" raw_path_position)
    if(NOT raw_path_position EQUAL -1)
        message(FATAL_ERROR "${command_name} rendered a cgroup path with a physical newline.")
    endif()

    assert_no_forbidden_terminal_text("${command_name}" "${output}")
endfunction()

function(assert_no_forbidden_terminal_text command_name output)
    foreach(forbidden IN ITEMS "${escape_character}" "${c1_control}" "${right_to_left_override}" "${zero_width_space}")
        string(FIND "${output}" "${forbidden}" forbidden_position)
        if(NOT forbidden_position EQUAL -1)
            message(FATAL_ERROR "${command_name} rendered forbidden terminal text.")
        endif()
    endforeach()
endfunction()

function(assert_safe_failure command_name result output error)
    if(result EQUAL 0)
        message(FATAL_ERROR "${command_name} unexpectedly succeeded.")
    endif()
    assert_no_forbidden_terminal_text("${command_name} standard output" "${output}")
    assert_no_forbidden_terminal_text("${command_name} standard error" "${error}")
endfunction()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" inspect evidence "${artifact_path}"
    RESULT_VARIABLE inspect_result OUTPUT_VARIABLE inspect_output ERROR_VARIABLE inspect_error)
if(NOT inspect_result EQUAL 0 OR NOT inspect_error STREQUAL "")
    message(FATAL_ERROR "Inspect command failed: ${inspect_error}")
endif()
assert_no_forbidden_terminal_text("Inspect command" "${inspect_output}")
string(FIND "${inspect_output}" [=[evidence\x1B.jsonl]=] inspect_escaped_path_position)
if(inspect_escaped_path_position EQUAL -1)
    message(FATAL_ERROR "Inspect command did not render the escaped artifact path.")
endif()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast evidence "${artifact_path}"
    RESULT_VARIABLE forecast_evidence_result OUTPUT_VARIABLE forecast_evidence_output ERROR_VARIABLE forecast_evidence_error)
if(NOT forecast_evidence_result EQUAL 0 OR NOT forecast_evidence_error STREQUAL "")
    message(FATAL_ERROR "Forecast evidence command failed: ${forecast_evidence_error}")
endif()
assert_no_forbidden_terminal_text("Forecast evidence command" "${forecast_evidence_output}")
string(FIND "${forecast_evidence_output}" [=[evidence\x1B.jsonl]=] forecast_evidence_escaped_path_position)
if(forecast_evidence_escaped_path_position EQUAL -1)
    message(FATAL_ERROR "Forecast evidence command did not render the escaped artifact path.")
endif()

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

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" inspect evidence "${poisoned_artifact_path}"
    RESULT_VARIABLE poisoned_inspect_result OUTPUT_VARIABLE poisoned_inspect_output ERROR_VARIABLE poisoned_inspect_error)
assert_safe_failure("Inspect poisoned artifact" "${poisoned_inspect_result}" "${poisoned_inspect_output}" "${poisoned_inspect_error}")
string(FIND "${poisoned_inspect_error}" "${expected_duplicate_path}" duplicate_path_position)
if(duplicate_path_position EQUAL -1)
    message(FATAL_ERROR "Inspect poisoned artifact did not render the escaped duplicate-key path.")
endif()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast evidence "${poisoned_artifact_path}"
    RESULT_VARIABLE poisoned_forecast_result OUTPUT_VARIABLE poisoned_forecast_output ERROR_VARIABLE poisoned_forecast_error)
assert_safe_failure("Forecast poisoned artifact" "${poisoned_forecast_result}" "${poisoned_forecast_output}" "${poisoned_forecast_error}")

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" trends analyze "${poisoned_manifest_path}"
    RESULT_VARIABLE poisoned_trends_result OUTPUT_VARIABLE poisoned_trends_output ERROR_VARIABLE poisoned_trends_error)
assert_safe_failure("Trends poisoned artifact" "${poisoned_trends_result}" "${poisoned_trends_output}" "${poisoned_trends_error}")
string(FIND "${poisoned_trends_error}" "${expected_manifest_duplicate_path}" trends_duplicate_path_position)
if(trends_duplicate_path_position EQUAL -1)
    message(FATAL_ERROR "Trends command did not render the escaped manifest duplicate-key path.")
endif()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast evaluate "${poisoned_manifest_path}"
    RESULT_VARIABLE poisoned_evaluation_result OUTPUT_VARIABLE poisoned_evaluation_output ERROR_VARIABLE poisoned_evaluation_error)
assert_safe_failure("Forecast evaluation poisoned artifact" "${poisoned_evaluation_result}" "${poisoned_evaluation_output}" "${poisoned_evaluation_error}")
string(FIND "${poisoned_evaluation_error}" "${expected_manifest_duplicate_path}" evaluation_duplicate_path_position)
if(evaluation_duplicate_path_position EQUAL -1)
    message(FATAL_ERROR "Forecast evaluation command did not render the escaped manifest duplicate-key path.")
endif()

file(REMOVE_RECURSE "${test_directory}")
