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

set(dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/dispatch.csv")
set(stdout_txt "${OPTIFLOW_TEST_OUTPUT_DIR}/stdout.txt")
set(corrupted_dispatch_csv "${OPTIFLOW_TEST_OUTPUT_DIR}/dispatch_corrupted.csv")
set(corrupt_script "${OPTIFLOW_TEST_OUTPUT_DIR}/corrupt_dispatch.py")

file(REMOVE "${dispatch_csv}" "${stdout_txt}" "${corrupted_dispatch_csv}" "${corrupt_script}")

execute_process(
    COMMAND "${OPTIFLOW_SOLVE}"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --output "${dispatch_csv}"
    RESULT_VARIABLE solve_result
    OUTPUT_VARIABLE solve_stdout
    ERROR_VARIABLE solve_stderr)

if(NOT solve_result EQUAL 0)
    message(FATAL_ERROR "optiflow_solve failed with code ${solve_result}\nstdout:\n${solve_stdout}\nstderr:\n${solve_stderr}")
endif()

file(WRITE "${stdout_txt}" "${solve_stdout}")

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}"
        "${OPTIFLOW_SOURCE_DIR}/tools/validate_dispatch.py"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --dispatch "${dispatch_csv}"
        --stdout "${stdout_txt}"
    RESULT_VARIABLE validation_result
    OUTPUT_VARIABLE validation_stdout
    ERROR_VARIABLE validation_stderr)

if(NOT validation_result EQUAL 0)
    message(FATAL_ERROR "dispatch validator rejected a valid dispatch\nstdout:\n${validation_stdout}\nstderr:\n${validation_stderr}")
endif()

if(NOT validation_stdout MATCHES "Dispatch validation passed")
    message(FATAL_ERROR "dispatch validator did not report success for a valid dispatch\nstdout:\n${validation_stdout}")
endif()

file(WRITE "${corrupt_script}" [=[
import csv
import sys

source_path = sys.argv[1]
destination_path = sys.argv[2]

with open(source_path, newline="") as source_file:
    reader = csv.DictReader(source_file)
    rows = list(reader)
    fieldnames = reader.fieldnames

if not fieldnames:
    raise SystemExit("dispatch header is missing")
if not rows:
    raise SystemExit("dispatch has no data rows")

rows[0]["net_power"] = str(float(rows[0]["net_power"]) + 1.0)

with open(destination_path, "w", newline="") as destination_file:
    writer = csv.DictWriter(destination_file, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(rows)
]=])

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}" "${corrupt_script}" "${dispatch_csv}" "${corrupted_dispatch_csv}"
    RESULT_VARIABLE corrupt_result
    OUTPUT_VARIABLE corrupt_stdout
    ERROR_VARIABLE corrupt_stderr)

if(NOT corrupt_result EQUAL 0)
    message(FATAL_ERROR "failed to create corrupted dispatch\nstdout:\n${corrupt_stdout}\nstderr:\n${corrupt_stderr}")
endif()

execute_process(
    COMMAND "${OPTIFLOW_PYTHON}"
        "${OPTIFLOW_SOURCE_DIR}/tools/validate_dispatch.py"
        --scenario "${OPTIFLOW_SOURCE_DIR}/examples/scenario.csv"
        --prices "${OPTIFLOW_SOURCE_DIR}/examples/prices.csv"
        --inflows "${OPTIFLOW_SOURCE_DIR}/examples/inflows.csv"
        --dispatch "${corrupted_dispatch_csv}"
        --stdout "${stdout_txt}"
    RESULT_VARIABLE corrupted_validation_result
    OUTPUT_VARIABLE corrupted_validation_stdout
    ERROR_VARIABLE corrupted_validation_stderr)

if(corrupted_validation_result EQUAL 0)
    message(FATAL_ERROR "dispatch validator accepted a corrupted dispatch\nstdout:\n${corrupted_validation_stdout}\nstderr:\n${corrupted_validation_stderr}")
endif()

if(NOT corrupted_validation_stderr MATCHES "net_power mismatch at index 0")
    message(FATAL_ERROR "dispatch validator failure did not identify the corrupted net_power field\nstdout:\n${corrupted_validation_stdout}\nstderr:\n${corrupted_validation_stderr}")
endif()
