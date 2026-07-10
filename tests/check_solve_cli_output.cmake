if(NOT DEFINED OPTIFLOW_SOLVE)
    message(FATAL_ERROR "OPTIFLOW_SOLVE is required")
endif()
if(NOT DEFINED OPTIFLOW_SOURCE_DIR)
    message(FATAL_ERROR "OPTIFLOW_SOURCE_DIR is required")
endif()
if(NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "OPTIFLOW_TEST_OUTPUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/dispatch.csv")
set(summary_json "${OPTIFLOW_TEST_OUTPUT_DIR}/summary.json")
file(REMOVE "${dispatch_csv}" "${summary_json}")

execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --output "${dispatch_csv}"
        --summary-output "${summary_json}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error)

if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "optiflow_solve failed with code ${command_result}\nstdout:\n${command_output}\nstderr:\n${command_error}")
endif()

set(required_output_patterns
    "Scenario: sample_day"
    "Time steps: 12"
    "Reservoir grid points: 21"
    "Action count: 12"
    "Solve seconds: [0-9]"
    "Simulation seconds: [0-9]"
    "Export energy MWh: [0-9]"
    "Import energy MWh: [0-9]"
    "Final reservoir volume: [0-9]"
    "Turbine steps: [0-9]"
    "Pump steps: [0-9]"
    "Spill steps: [0-9]"
    "Wait steps: [0-9]"
    "Cumulative profit: "
    "Dispatch written to: ")
foreach(pattern IN LISTS required_output_patterns)
    string(REGEX MATCH "${pattern}" match_result "${command_output}")
    if(match_result STREQUAL "")
        message(FATAL_ERROR "CLI output does not contain required pattern: ${pattern}\nstdout:\n${command_output}")
    endif()
endforeach()

if(NOT EXISTS "${dispatch_csv}")
    message(FATAL_ERROR "Expected dispatch CSV was not created: ${dispatch_csv}")
endif()
file(READ "${dispatch_csv}" dispatch_contents)
string(FIND "${dispatch_contents}" "time_index,timestamp_utc,price,natural_inflow,reservoir_volume,turbine_flow" header_position)
if(header_position EQUAL -1)
    message(FATAL_ERROR "Dispatch CSV header is missing or was changed")
endif()

if(NOT EXISTS "${summary_json}")
    message(FATAL_ERROR "Expected summary JSON was not created: ${summary_json}")
endif()
file(READ "${summary_json}" summary_contents)
set(required_summary_patterns
    "\"cumulative_profit\": [0-9.-]"
    "\"export_energy_mwh\": [0-9.-]"
    "\"import_energy_mwh\": [0-9.-]"
    "\"final_reservoir_volume\": [0-9.-]"
    "\"solve_seconds\": [0-9.-]"
    "\"simulation_seconds\": [0-9.-]"
    "\"wait_steps\": [0-9]")
foreach(pattern IN LISTS required_summary_patterns)
    string(REGEX MATCH "${pattern}" match_result "${summary_contents}")
    if(match_result STREQUAL "")
        message(FATAL_ERROR "Summary JSON does not contain required pattern: ${pattern}\nJSON:\n${summary_contents}")
    endif()
endforeach()
