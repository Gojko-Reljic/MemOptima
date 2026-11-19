if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef artifact_suffix)
set(test_directory "${CMAKE_CURRENT_BINARY_DIR}/memoptima-trends-${artifact_suffix}")
file(MAKE_DIRECTORY "${test_directory}")
set(artifact_path "${test_directory}/evidence.jsonl")
set(manifest_path "${test_directory}/manifest.json")
set(invalid_manifest_path "${test_directory}/invalid-manifest.json")

file(WRITE "${artifact_path}"
"{\"record_type\":\"session_start\",\"schema_version\":1,\"application_version\":\"0.7.0\",\"started_at_utc\":\"2026-01-01T00:00:00.000Z\",\"requested_sample_count\":3,\"requested_interval_ms\":\"10000\",\"output_max_bytes\":\"536870912\",\"output_permissions_octal\":\"0600\",\"flush_after_each_record\":true,\"host_identifiers_included\":false}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":1,\"planned_offset_ms\":\"0\",\"captured_at_utc\":\"2026-01-01T00:00:00.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":2,\"planned_offset_ms\":\"10000\",\"captured_at_utc\":\"2026-01-01T00:00:10.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":3,\"planned_offset_ms\":\"20000\",\"captured_at_utc\":\"2026-01-01T00:00:20.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"session_end\",\"schema_version\":1,\"outcome\":\"completed\",\"completed_sample_count\":3,\"ended_at_utc\":\"2026-01-01T00:00:20.001Z\",\"failure\":null}\n")
file(WRITE "${manifest_path}" "{\"manifest_version\":1,\"artifacts\":[{\"path\":\"evidence.jsonl\"}]}")
file(WRITE "${invalid_manifest_path}" "not JSON")

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" trends analyze "${manifest_path}"
    RESULT_VARIABLE success_result OUTPUT_VARIABLE success_output ERROR_VARIABLE success_error)
if(NOT success_result EQUAL 0 OR NOT success_error STREQUAL "")
    message(FATAL_ERROR "Trends command failed: ${success_error}")
endif()
if(NOT success_output STREQUAL "No container observation epochs found in manifest.\n")
    message(FATAL_ERROR "Unexpected empty trends output: ${success_output}")
endif()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" trends analyze "${invalid_manifest_path}"
    RESULT_VARIABLE invalid_result OUTPUT_VARIABLE invalid_output ERROR_VARIABLE invalid_error)
if(NOT invalid_result EQUAL 1 OR NOT invalid_output STREQUAL "")
    message(FATAL_ERROR "Invalid trend manifest error contract failed.")
endif()
string(FIND "${invalid_error}" "Error: unable to load trend manifest:" invalid_position)
if(invalid_position EQUAL -1)
    message(FATAL_ERROR "Invalid trend manifest diagnostic is unclear: ${invalid_error}")
endif()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" trends analyze
    RESULT_VARIABLE usage_result OUTPUT_VARIABLE usage_output ERROR_VARIABLE usage_error)
if(NOT usage_result EQUAL 2 OR NOT usage_output STREQUAL "")
    message(FATAL_ERROR "Trends usage error contract failed.")
endif()
string(FIND "${usage_error}" "'trends' requires 'analyze <MANIFEST>'" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "Trends usage error is unclear: ${usage_error}")
endif()

file(REMOVE_RECURSE "${test_directory}")
