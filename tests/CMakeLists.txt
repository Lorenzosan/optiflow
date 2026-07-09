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


if(OPTIFLOW_BUILD_GRPC)
    add_executable(optiflow_proto_conversion_tests
        test_proto_conversion.cpp)

    target_link_libraries(optiflow_proto_conversion_tests
        PRIVATE
            optiflow::optimizer_service
            optiflow_project_warnings)

    target_compile_features(optiflow_proto_conversion_tests PRIVATE cxx_std_20)

    add_test(NAME optiflow_proto_conversion_tests COMMAND optiflow_proto_conversion_tests)

    add_executable(optiflow_optimizer_service_smoke_tests
        test_optimizer_service_smoke.cpp)

    target_link_libraries(optiflow_optimizer_service_smoke_tests
        PRIVATE
            optiflow::optimizer_service
            optiflow_project_warnings)

    target_compile_features(optiflow_optimizer_service_smoke_tests PRIVATE cxx_std_20)

    add_test(NAME optiflow_optimizer_service_smoke_tests
             COMMAND optiflow_optimizer_service_smoke_tests)
endif()
