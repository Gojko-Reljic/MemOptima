if(NOT DEFINED MEMOPTIMA_EXECUTABLE)
    message(FATAL_ERROR "MEMOPTIMA_EXECUTABLE must be provided.")
endif()

string(RANDOM LENGTH 16 ALPHABET 0123456789abcdef artifact_suffix)
set(artifact_path "${CMAKE_CURRENT_BINARY_DIR}/memoptima-forecast-${artifact_suffix}.jsonl")

file(WRITE "${artifact_path}"
"{\"record_type\":\"session_start\",\"schema_version\":1,\"application_version\":\"0.6.0\",\"started_at_utc\":\"2026-11-14T12:00:00.000Z\",\"requested_sample_count\":3,\"requested_interval_ms\":\"10000\",\"output_max_bytes\":\"536870912\",\"output_permissions_octal\":\"0600\",\"flush_after_each_record\":true,\"host_identifiers_included\":false}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":1,\"planned_offset_ms\":\"0\",\"captured_at_utc\":\"2026-11-14T12:00:00.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":2,\"planned_offset_ms\":\"10000\",\"captured_at_utc\":\"2026-11-14T12:00:10.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"sample\",\"schema_version\":1,\"sample_sequence\":3,\"planned_offset_ms\":\"20000\",\"captured_at_utc\":\"2026-11-14T12:00:20.000Z\",\"schedule_delay_ms\":\"0\",\"skipped_container_count\":\"0\",\"discovery_warnings\":[],\"containers\":[]}\n"
"{\"record_type\":\"session_end\",\"schema_version\":1,\"outcome\":\"completed\",\"completed_sample_count\":3,\"ended_at_utc\":\"2026-11-14T12:00:20.001Z\",\"failure\":null}\n")

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast evidence "${artifact_path}"
    RESULT_VARIABLE forecast_result OUTPUT_VARIABLE forecast_output ERROR_VARIABLE forecast_error)
if(NOT forecast_result EQUAL 0 OR NOT forecast_error STREQUAL "")
    message(FATAL_ERROR "Forecast command failed: ${forecast_error}")
endif()
foreach(expected_text "Evidence forecast" "Samples: 3" "CONTAINER ID" "not an OOM event or memory leak")
    string(FIND "${forecast_output}" "${expected_text}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "Forecast output is missing: ${expected_text}")
    endif()
endforeach()

execute_process(COMMAND "${MEMOPTIMA_EXECUTABLE}" forecast
    RESULT_VARIABLE usage_result OUTPUT_VARIABLE usage_output ERROR_VARIABLE usage_error)
if(NOT usage_result EQUAL 2 OR NOT usage_output STREQUAL "")
    message(FATAL_ERROR "Forecast usage error contract failed.")
endif()
string(FIND "${usage_error}" "'forecast' requires 'evidence <FILE>' or 'evaluate <MANIFEST>'" usage_position)
if(usage_position EQUAL -1)
    message(FATAL_ERROR "Forecast usage error is unclear: ${usage_error}")
endif()

file(REMOVE "${artifact_path}")
