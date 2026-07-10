add_executable(optiflow_tests
    test_core.cpp)

target_link_libraries(optiflow_tests
    PRIVATE
        optiflow_optimization
        optiflow_project_warnings)

target_compile_features(optiflow_tests PRIVATE cxx_std_20)

add_test(NAME optiflow_tests COMMAND optiflow_tests)


add_executable(optiflow_simple_behaviour_tests
    test_simple_behaviour.cpp)

target_link_libraries(optiflow_simple_behaviour_tests
    PRIVATE
        optiflow_optimization
        optiflow_project_warnings)

target_compile_features(optiflow_simple_behaviour_tests PRIVATE cxx_std_20)

add_test(NAME optiflow_simple_behaviour_tests COMMAND optiflow_simple_behaviour_tests)


add_executable(optiflow_optimizer_oracle_tests
    test_optimizer_oracles.cpp)

target_link_libraries(optiflow_optimizer_oracle_tests
    PRIVATE
        optiflow_optimization
        optiflow_project_warnings)

target_compile_features(optiflow_optimizer_oracle_tests PRIVATE cxx_std_20)

add_test(NAME optiflow_optimizer_oracle_tests COMMAND optiflow_optimizer_oracle_tests)


add_test(NAME optiflow_solve_cli_example
    COMMAND ${CMAKE_COMMAND}
        -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
        -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/solve-cli
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_solve_cli_output.cmake)

add_test(NAME optiflow_solve_cli_invalid_input
    COMMAND ${CMAKE_COMMAND}
        -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
        -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/solve-cli-invalid-input
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_solve_cli_invalid_input.cmake)

add_test(NAME optiflow_solve_cli_validate_only
    COMMAND ${CMAKE_COMMAND}
        -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
        -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_solve_cli_validate_only.cmake)

find_package(Python3 COMPONENTS Interpreter)

add_test(NAME optiflow_solve_cli_yearly_example
    COMMAND ${CMAKE_COMMAND}
        -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
        -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
        -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/solve-cli-yearly
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_yearly_example.cmake)


if(Python3_Interpreter_FOUND)
    add_test(NAME optiflow_yearly_dispatch_summary
        COMMAND ${CMAKE_COMMAND}
            -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
            -DOPTIFLOW_PYTHON=${Python3_EXECUTABLE}
            -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/yearly-summary
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_yearly_summary.cmake)

    add_test(NAME optiflow_dispatch_validator_regression
        COMMAND ${CMAKE_COMMAND}
            -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
            -DOPTIFLOW_PYTHON=${Python3_EXECUTABLE}
            -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/dispatch-validator
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_dispatch_validator_regression.cmake)

    add_test(NAME optiflow_deterministic_hydro_oracles
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/check_deterministic_hydro_oracles.py
            --solve $<TARGET_FILE:optiflow_solve>
            --validator ${PROJECT_SOURCE_DIR}/tools/validate_dispatch.py
            --source-dir ${PROJECT_SOURCE_DIR}
            --output-dir ${PROJECT_BINARY_DIR}/test-output/deterministic-hydro-oracles)

    add_test(NAME optiflow_yearly_scenario_comparison
        COMMAND ${CMAKE_COMMAND}
            -DOPTIFLOW_SOLVE=$<TARGET_FILE:optiflow_solve>
            -DOPTIFLOW_PYTHON=${Python3_EXECUTABLE}
            -DOPTIFLOW_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DOPTIFLOW_TEST_OUTPUT_DIR=${PROJECT_BINARY_DIR}/test-output/yearly-comparison
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_yearly_scenario_comparison.cmake)
endif()
