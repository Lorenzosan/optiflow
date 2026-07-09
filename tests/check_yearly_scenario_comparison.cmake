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
if(NOT compare_stdout MATCHES "Action grid")
    message(FATAL_ERROR "comparison stdout is missing the action-grid column label")
endif()
if(compare_stdout MATCHES "Delta vs")
    message(FATAL_ERROR "comparison stdout should not mention a scenario-specific delta base")
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

string(REPLACE "\r\n" "\n" normalized_summary_text "${summary_text}")
string(REPLACE "\n" ";" summary_lines "${normalized_summary_text}")

function(find_summary_row output_variable scenario_name)
    foreach(line IN LISTS summary_lines)
        if(line MATCHES "^${scenario_name},")
            set("${output_variable}" "${line}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "comparison output is missing row for ${scenario_name}")
endfunction()

function(require_csv_field fields_variable index expected label)
    list(GET ${fields_variable} ${index} actual)
    if(NOT actual STREQUAL "${expected}")
        message(FATAL_ERROR "${label}: expected ${expected}, got ${actual}")
    endif()
endfunction()

find_summary_row(base_row "synthetic_year")
find_summary_row(no_battery_row "synthetic_year_no_battery")
find_summary_row(high_degradation_row "synthetic_year_high_battery_degradation")

string(REPLACE "," ";" base_fields "${base_row}")
string(REPLACE "," ";" no_battery_fields "${no_battery_row}")
string(REPLACE "," ";" high_degradation_fields "${high_degradation_row}")

require_csv_field(base_fields 2 "2195040" "base scenario profit")
require_csv_field(base_fields 8 "50" "base scenario initial battery SOC")
require_csv_field(base_fields 9 "50" "base scenario final battery SOC")
require_csv_field(base_fields 13 "1460" "base scenario battery charge steps")
require_csv_field(base_fields 14 "1460" "base scenario battery discharge steps")
require_csv_field(base_fields 18 "5" "base scenario battery grid points")
require_csv_field(base_fields 19 "72" "base scenario action grid size")

require_csv_field(no_battery_fields 2 "1569600" "no-battery scenario profit")
require_csv_field(no_battery_fields 8 "0" "no-battery scenario initial battery SOC")
require_csv_field(no_battery_fields 9 "0" "no-battery scenario final battery SOC")
require_csv_field(no_battery_fields 13 "0" "no-battery scenario battery charge steps")
require_csv_field(no_battery_fields 14 "0" "no-battery scenario battery discharge steps")
require_csv_field(no_battery_fields 18 "1" "no-battery scenario battery grid points")
require_csv_field(no_battery_fields 19 "18" "no-battery scenario action grid size")

require_csv_field(high_degradation_fields 2 "1569600" "high-degradation scenario profit")
require_csv_field(high_degradation_fields 8 "50" "high-degradation scenario initial battery SOC")
require_csv_field(high_degradation_fields 9 "50" "high-degradation scenario final battery SOC")
require_csv_field(high_degradation_fields 13 "0" "high-degradation scenario battery charge steps")
require_csv_field(high_degradation_fields 14 "0" "high-degradation scenario battery discharge steps")
require_csv_field(high_degradation_fields 18 "5" "high-degradation scenario battery grid points")
require_csv_field(high_degradation_fields 19 "72" "high-degradation scenario action grid size")
