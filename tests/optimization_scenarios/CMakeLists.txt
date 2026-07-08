add_executable(optiflow_scenario_tests
  main.cpp
)

target_link_libraries(optiflow_scenario_tests
  PRIVATE
    optiflow::demo
)

add_test(NAME optiflow_scenario_tests COMMAND optiflow_scenario_tests)
