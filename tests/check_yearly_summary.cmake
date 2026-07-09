if(NOT DEFINED OPTIFLOW_SOLVE)
    message(FATAL_ERROR "OPTIFLOW_SOLVE is required")
endif()
if(NOT DEFINED OPTIFLOW_PYTHON)
    message(FATAL_ERROR "OPTIFLOW_PYTHON is required")
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
set(summary_tool "${OPTIFLOW_SOURCE_DIR}/tools/summarize_dispatch.py")

foreach(input_file IN ITEMS "${scenario_csv}" "${prices_csv}" "${inflows_csv}" "${summary_tool}")
    if(NOT EXISTS "${input_file}")
        message(FATAL_ERROR "Missing yearly summary input file: ${input_file}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/yearly_dispatch.csv")
file(REMOVE "${dispatch_csv}")

execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${scenario_csv}"
        --prices "${prices_csv}"
        --inflows "${inflows_csv}"
        --output "${dispatch_csv}"
    RESULT_VARIABLE solve_result
    OUTPUT_VARIABLE solve_output
    ERROR_VARIABLE solve_error)

if(NOT solve_result EQUAL 0)
    message(FATAL_ERROR "optiflow_solve failed for yearly summary test with code ${solve_result}\nstdout:\n${solve_output}\nstderr:\n${solve_error}")
endif()

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}" "${summary_tool}"
        --scenario "${scenario_csv}"
        --prices "${prices_csv}"
        --inflows "${inflows_csv}"
        --dispatch "${dispatch_csv}"
    RESULT_VARIABLE summary_result
    OUTPUT_VARIABLE summary_output
    ERROR_VARIABLE summary_error)

if(NOT summary_result EQUAL 0)
    message(FATAL_ERROR "summarize_dispatch.py failed with code ${summary_result}\nstdout:\n${summary_output}\nstderr:\n${summary_error}")
endif()

set(required_output_patterns
    "OptiFlow dispatch summary"
    "Scenario: synthetic_year"
    "Rows: 8760"
    "Export revenue:"
    "Import cost:"
    "Net market cashflow:"
    "Operating cost:"
    "Battery degradation cost:"
    "Recomputed reward:"
    "Reported cumulative profit:"
    "Export energy:"
    "Import energy:"
    "Net export energy:"
    "Average export price:"
    "Average import price:"
    "Final reservoir volume:"
    "Final battery SOC:"
    "Terminal reservoir target deviation:"
    "Terminal battery target deviation:")

foreach(pattern IN LISTS required_output_patterns)
    string(FIND "${summary_output}" "${pattern}" match_position)
    if(match_position EQUAL -1)
        message(FATAL_ERROR "Yearly summary output does not contain required text: ${pattern}\nstdout:\n${summary_output}")
    endif()
endforeach()
