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

file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")

set(summary_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/yearly_comparison.csv")

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}"
        "${OPTIFLOW_SOURCE_DIR}/tools/compare_scenarios.py"
        --solve "${OPTIFLOW_SOLVE}"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/yearly/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/yearly/inflows.csv"
        --output-dir "${OPTIFLOW_TEST_OUTPUT_DIR}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario.csv"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario_no_battery.csv"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/yearly/scenario_high_battery_degradation.csv"
        --summary-output "${summary_csv}"
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_stdout
    ERROR_VARIABLE compare_stderr)

if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "scenario comparison failed: ${compare_stderr}\n${compare_stdout}")
endif()

if(NOT EXISTS "${summary_csv}")
    message(FATAL_ERROR "scenario comparison did not create ${summary_csv}")
endif()

if(NOT compare_stdout MATCHES "Scenario comparison")
    message(FATAL_ERROR "comparison stdout is missing the readable table heading")
endif()
if(NOT compare_stdout MATCHES "Delta column is measured relative to the first scenario")
    message(FATAL_ERROR "comparison stdout is missing the generic delta explanation")
endif()

file(READ "${summary_csv}" summary_text)

if(NOT summary_text MATCHES "scenario,rows,cumulative_profit")
    message(FATAL_ERROR "comparison output is missing the expected CSV header")
endif()
if(NOT summary_text MATCHES "synthetic_year")
    message(FATAL_ERROR "comparison output is missing the base yearly scenario")
endif()
if(NOT summary_text MATCHES "synthetic_year_no_battery")
    message(FATAL_ERROR "comparison output is missing the no-battery yearly scenario")
endif()
if(NOT summary_text MATCHES "synthetic_year_no_battery,[^\n]*,0,0,")
    message(FATAL_ERROR "no-battery scenario should start and finish with zero battery SOC")
endif()
if(NOT summary_text MATCHES "synthetic_year_high_battery_degradation")
    message(FATAL_ERROR "comparison output is missing the high-degradation battery scenario")
endif()
