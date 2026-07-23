if(NOT DEFINED OPTIFLOW_SOLVE OR NOT DEFINED OPTIFLOW_PYTHON OR
   NOT DEFINED OPTIFLOW_SOURCE_DIR OR NOT DEFINED OPTIFLOW_TEST_OUTPUT_DIR)
    message(FATAL_ERROR "Required variables are missing")
endif()

file(REMOVE_RECURSE "${OPTIFLOW_TEST_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OPTIFLOW_TEST_OUTPUT_DIR}")
set(summary_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/resolution_analysis.csv")

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}" "${OPTIFLOW_SOURCE_DIR}/tools/analyze_resolution.py"
        --solve "${OPTIFLOW_SOLVE}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --output-dir "${OPTIFLOW_TEST_OUTPUT_DIR}"
        --summary-output "${summary_csv}"
        --resolution 5,2,2,2
        --resolution 9,3,3,3
        --resolution 17,5,5,5
    RESULT_VARIABLE analysis_result
    OUTPUT_VARIABLE analysis_stdout
    ERROR_VARIABLE analysis_stderr)
if(NOT analysis_result EQUAL 0)
    message(FATAL_ERROR "resolution analysis failed: ${analysis_stderr}\n${analysis_stdout}")
endif()
if(NOT analysis_stdout MATCHES "Resolution sensitivity")
    message(FATAL_ERROR "resolution analysis stdout is missing the heading")
endif()
if(NOT analysis_stdout MATCHES "not assumed to improve monotonically")
    message(FATAL_ERROR "resolution analysis stdout is missing the interpretation warning")
endif()

file(READ "${summary_csv}" summary_text)
if(NOT summary_text MATCHES "case,reservoir_grid_points,turbine_flow_steps")
    message(FATAL_ERROR "resolution analysis output is missing the expected CSV header")
endif()
foreach(expected_case IN ITEMS
        "r5_t2_s2_p2,5,2,2,2,5"
        "r9_t3_s3_p3,9,3,3,3,11"
        "r17_t5_s5_p5,17,5,5,5,29")
    if(NOT summary_text MATCHES "${expected_case}")
        message(FATAL_ERROR "resolution analysis output is missing ${expected_case}")
    endif()
endforeach()
if(NOT summary_text MATCHES "r17_t5_s5_p5,17,5,5,5,[0-9]+,[^,]+,0[.]0")
    message(FATAL_ERROR "finest resolution does not have zero cashflow delta")
endif()

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}" "${OPTIFLOW_SOURCE_DIR}/tools/analyze_resolution.py"
        --solve "${OPTIFLOW_SOLVE}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --output-dir "${OPTIFLOW_TEST_OUTPUT_DIR}/invalid"
        --resolution 5,2,1,2
        --resolution 8,3,1,3
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_stderr)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "non-nested resolution analysis unexpectedly succeeded")
endif()
if(NOT invalid_stderr MATCHES "not nested")
    message(FATAL_ERROR "non-nested resolution error is missing: ${invalid_stderr}")
endif()
