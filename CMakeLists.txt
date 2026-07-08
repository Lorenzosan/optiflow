cmake_minimum_required(VERSION 3.20)

project(OptiFlow
    VERSION 0.1.0
    DESCRIPTION "Local pumped-storage and battery dispatch optimization demo"
    LANGUAGES CXX)

option(OPTIFLOW_BUILD_TESTS "Build OptiFlow tests" ON)

add_subdirectory(libs/optimization)
add_subdirectory(apps/solve_cli)

if(OPTIFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
