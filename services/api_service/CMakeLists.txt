add_executable(optiflow_api_service src/main.cpp)
target_link_libraries(optiflow_api_service PRIVATE optiflow::demo optiflow::service_common)
