add_library(optiflow_optimizer_service STATIC
    src/ProtoConversion.cpp)

add_library(optiflow::optimizer_service ALIAS optiflow_optimizer_service)

target_include_directories(optiflow_optimizer_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(optiflow_optimizer_service
    PUBLIC
        optiflow_optimization
        optiflow_optimizer_proto
    PRIVATE
        optiflow_project_warnings)

target_compile_features(optiflow_optimizer_service PUBLIC cxx_std_20)
