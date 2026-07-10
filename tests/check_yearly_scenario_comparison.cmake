if(NOT DEFINED OPTIFLOW_SOLVE OR NOT DEFINED OPTIFLOW_PYTHON OR
   NOT DEFINED OPTIFLOW_SOURCE_DIR OR NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "Required variables are missing")
endif()
file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(summary_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/yearly_comparison.csv")
execute_process(
    COMMAND "${OPTIFLOW_PYTHON}" "${OPTIFLOW_SOURCE_DIR}/tools/compare_scenarios.py"
        --solve "${OPTIFLOW_SOLVE}"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/yearly/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/yearly/inflows.csv"
        --output-dir "${OPTIFLOW_TEST_OUTPUT_DIR}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario.csv"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario_no_pumping.csv"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario_high_operating_cost.csv"
        --summary-output "${summary_csv}"
    RESULT_VARIABLE compare_result OUTPUT_VARIABLE compare_stdout ERROR_VARIABLE compare_stderr)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "scenario comparison failed: ${compare_stderr}\n${compare_stdout}")
endif()
if(NOT compare_stdout MATCHES "Scenario comparison")
    message(FATAL_ERROR "comparison stdout is missing the heading")
endif()
file(READ "${summary_csv}" summary_text)
if(NOT summary_text MATCHES "scenario,rows,cumulative_profit")
    message(FATAL_ERROR "comparison output is missing the expected CSV header")
endif()
foreach(name IN ITEMS synthetic_year synthetic_year_no_pumping synthetic_year_high_operating_cost)
    if(NOT summary_text MATCHES "${name}")
        message(FATAL_ERROR "comparison output is missing ${name}")
    endif()
endforeach()
