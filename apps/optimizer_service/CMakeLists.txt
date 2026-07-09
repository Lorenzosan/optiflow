add_executable(optiflow_optimizer_service_server main.cpp)

add_executable(optiflow::optimizer_service_server ALIAS optiflow_optimizer_service_server)

target_link_libraries(optiflow_optimizer_service_server
    PRIVATE
        optiflow::optimizer_service
        optiflow_project_warnings)

target_compile_features(optiflow_optimizer_service_server PRIVATE cxx_std_20)
