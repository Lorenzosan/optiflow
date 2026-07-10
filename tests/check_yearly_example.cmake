if(NOT DEFINED OPTIFLOW_SOLVE)
    message(FATAL_ERROR "OPTIFLOW_SOLVE is required")
endif()
if(NOT DEFINED OPTIFLOW_SOURCE_DIR)
    message(FATAL_ERROR "OPTIFLOW_SOURCE_DIR is required")
endif()
if(NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "OPTIFLOW_TEST_OUTPUT_DIR is required")
endif()

set(scenario_csv "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario.csv")
set(prices_csv "${OPTIFLOW_SOURCE_DIR}/examples/yearly/prices.csv")
set(inflows_csv "${OPTIFLOW_SOURCE_DIR}/examples/yearly/inflows.csv")
foreach(input_file IN ITEMS "${scenario_csv}" "${prices_csv}" "${inflows_csv}")
    if(NOT EXISTS "${input_file}")
        message(FATAL_ERROR "Missing yearly example input file: ${input_file}")
    endif()
endforeach()

file(STRINGS "${prices_csv}" price_rows)
list(LENGTH price_rows price_row_count)
if(NOT price_row_count EQUAL 8761)
    message(FATAL_ERROR "Yearly prices file must contain one header plus 8760 rows; got ${price_row_count}")
endif()
file(STRINGS "${inflows_csv}" inflow_rows)
list(LENGTH inflow_rows inflow_row_count)
if(NOT inflow_row_count EQUAL 8761)
    message(FATAL_ERROR "Yearly inflows file must contain one header plus 8760 rows; got ${inflow_row_count}")
endif()

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/yearly_dispatch.csv")
file(REMOVE "${dispatch_csv}")
execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${scenario_csv}"
        --prices "${prices_csv}"
        --inflows "${inflows_csv}"
        --output "${dispatch_csv}"
    RESULT_VARIABLE command_result
    OUTPUT_VARIABLE command_output
    ERROR_VARIABLE command_error)
if(NOT command_result EQUAL 0)
    message(FATAL_ERROR "optiflow_solve failed for yearly example with code ${command_result}\nstdout:\n${command_output}\nstderr:\n${command_error}")
endif()

set(required_output_patterns
    "Scenario: synthetic_year"
    "Time steps: 8760"
    "Reservoir grid points: 9"
    "Action count: 10"
    "Solve seconds: [0-9]"
    "Simulation seconds: [0-9]"
    "Cumulative profit: ")
foreach(pattern IN LISTS required_output_patterns)
    string(REGEX MATCH "${pattern}" match_result "${command_output}")
    if(match_result STREQUAL "")
        message(FATAL_ERROR "Yearly CLI output does not contain required pattern: ${pattern}\nstdout:\n${command_output}")
    endif()
endforeach()

file(STRINGS "${dispatch_csv}" dispatch_rows)
list(LENGTH dispatch_rows dispatch_row_count)
if(NOT dispatch_row_count EQUAL 8761)
    message(FATAL_ERROR "Yearly dispatch CSV must contain one header plus 8760 rows; got ${dispatch_row_count}")
endif()
