add_executable(optiflow_optimizer_service src/main.cpp)
target_link_libraries(optiflow_optimizer_service PRIVATE optiflow::demo optiflow::service_common)
