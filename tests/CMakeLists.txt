add_executable(optiflow_tests
    test_core.cpp)

target_link_libraries(optiflow_tests PRIVATE optiflow_optimization)

target_compile_features(optiflow_tests PRIVATE cxx_std_20)

add_test(NAME optiflow_tests COMMAND optiflow_tests)
