if(NOT DEFINED OPTIFLOW_SOLVE)
    message(FATAL_ERROR "OPTIFLOW_SOLVE is required")
endif()
if(NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "OPTIFLOW_TEST_OUTPUT_DIR is required")
endif()

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")

set(scenario_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/invalid_scenario.csv")
set(prices_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/prices.csv")
set(inflows_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/inflows.csv")
set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/dispatch.csv")

file(REMOVE "${scenario_csv}" "${prices_csv}" "${inflows_csv}" "${dispatch_csv}")

file(WRITE "${prices_csv}" "timestamp_utc,price\n2027-01-01T00:00:00Z,100\n")
file(WRITE "${inflows_csv}" "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,0\n")
file(WRITE "${scenario_csv}" "key,value\nscenario_name,first\nscenario_name,second\n")

execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${scenario_csv}"
        --prices "${prices_csv}"
        --inflows "${inflows_csv}"
        --output "${dispatch_csv}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error)

if(command_result EQUAL 0)
    message(FATAL_ERROR "invalid scenario unexpectedly succeeded\nstdout:\n${command_output}\nstderr:\n${command_error}")
endif()

if(NOT command_error MATCHES "Error:")
    message(FATAL_ERROR "invalid scenario stderr is missing the error prefix\nstderr:\n${command_error}")
endif()
if(NOT command_error MATCHES "duplicate scenario key")
    message(FATAL_ERROR "invalid scenario stderr is missing the parse failure reason\nstderr:\n${command_error}")
endif()
if(NOT command_error MATCHES "Usage:")
    message(FATAL_ERROR "invalid scenario stderr is missing CLI usage text\nstderr:\n${command_error}")
endif()

if(NOT command_output STREQUAL "")
    message(FATAL_ERROR "invalid scenario should not write normal stdout\nstdout:\n${command_output}")
endif()

if(EXISTS "${dispatch_csv}")
    message(FATAL_ERROR "invalid scenario should not create a dispatch CSV: ${dispatch_csv}")
endif()
