if(NOT DEFINED OPTIFLOW_SOLVE)
    message(FATAL_ERROR "OPTIFLOW_SOLVE is required")
endif()
if(NOT DEFINED OPTIFLOW_SOURCE_DIR)
    message(FATAL_ERROR "OPTIFLOW_SOURCE_DIR is required")
endif()
if(NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "OPTIFLOW_TEST_OUTPUT_DIR is required")
endif()

set(example_dir "${OPTIFLOW_SOURCE_DIR}/examples/multistep")
set(scenario_csv "${example_dir}/scenario.csv")
set(prices_csv "${example_dir}/prices.csv")
set(inflows_csv "${example_dir}/inflows.csv")
foreach(input_file IN ITEMS "${scenario_csv}" "${prices_csv}" "${inflows_csv}")
    if(NOT EXISTS "${input_file}")
        message(FATAL_ERROR "Missing multistep example input file: ${input_file}")
    endif()
endforeach()

file(STRINGS "${prices_csv}" price_rows)
file(STRINGS "${inflows_csv}" inflow_rows)
list(LENGTH price_rows price_row_count)
list(LENGTH inflow_rows inflow_row_count)
if(NOT price_row_count EQUAL 13 OR NOT inflow_row_count EQUAL 13)
    message(FATAL_ERROR
        "Multistep inputs must each contain one header plus 12 rows; "
        "got prices=${price_row_count}, inflows=${inflow_row_count}")
endif()

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/dispatch.csv")
set(summary_json "${OPTIFLOW_TEST_OUTPUT_DIR}/summary.json")
file(REMOVE "${dispatch_csv}" "${summary_json}")

execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${scenario_csv}"
        --prices "${prices_csv}"
        --inflows "${inflows_csv}"
        --output "${dispatch_csv}"
        --summary-output "${summary_json}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR
        "optiflow_solve failed for multistep example with code ${command_result}\n"
        "stdout:\n${command_output}\nstderr:\n${command_error}")
endif()

set(required_output_patterns
    "Scenario: multistep_inflow_pulse"
    "Time steps: 12"
    "Reservoir grid points: 5"
    "Action count: 6"
    "Export energy .MWh.: 135"
    "Import energy .MWh.: 0"
    "Final reservoir content .MWh hydraulic.: 0"
    "Turbine steps: 6"
    "Pump steps: 0"
    "Spill steps: 0"
    "Wait steps: 6"
    "Net operating cashflow .€.: 202.5")
foreach(pattern IN LISTS required_output_patterns)
    string(REGEX MATCH "${pattern}" match_result "${command_output}")
    if(match_result STREQUAL "")
        message(FATAL_ERROR
            "Multistep CLI output does not contain required pattern: ${pattern}\n"
            "stdout:\n${command_output}")
    endif()
endforeach()

file(STRINGS "${dispatch_csv}" dispatch_rows)
list(LENGTH dispatch_rows dispatch_row_count)
if(NOT dispatch_row_count EQUAL 13)
    message(FATAL_ERROR
        "Multistep dispatch must contain one header plus 12 rows; got ${dispatch_row_count}")
endif()

file(READ "${summary_json}" summary_contents)
set(required_summary_patterns
    "\"net_operating_cashflow\": 202.5"
    "\"export_energy_mwh\": 135"
    "\"import_energy_mwh\": 0"
    "\"final_reservoir_volume\": 0"
    "\"turbine_steps\": 6"
    "\"pump_steps\": 0"
    "\"spill_steps\": 0"
    "\"wait_steps\": 6")
foreach(pattern IN LISTS required_summary_patterns)
    string(FIND "${summary_contents}" "${pattern}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR
            "Multistep summary does not contain required text: ${pattern}\n"
            "JSON:\n${summary_contents}")
    endif()
endforeach()
