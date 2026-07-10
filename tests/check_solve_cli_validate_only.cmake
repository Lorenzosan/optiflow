execute_process(
    COMMAND ${OPTIFLOW_SOLVE}
        --scenario ${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv
        --prices ${OPTIFLOW_SOURCE_DIR}/examples/prices.csv
        --inflows ${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv
        --validate-only
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr)

if(NOT result EQUAL 0)
    message(FATAL_ERROR "validate-only command failed (${result})\nstdout:\n${stdout}\nstderr:\n${stderr}")
endif()

if(NOT stdout MATCHES "Scenario valid: sample_day")
    message(FATAL_ERROR "validate-only output did not confirm the scenario name\nstdout:\n${stdout}")
endif()
