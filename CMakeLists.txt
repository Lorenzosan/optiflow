cmake_minimum_required(VERSION 3.20)

project(OptiFlow
    VERSION 0.1.0
    DESCRIPTION "Local pumped-storage and battery dispatch optimization demo"
    LANGUAGES CXX)

option(OPTIFLOW_BUILD_TESTS "Build OptiFlow tests" ON)
option(OPTIFLOW_BUILD_GRPC "Build protobuf/gRPC boundary targets" OFF)
option(OPTIFLOW_ENABLE_WARNINGS "Enable compiler warnings" ON)

add_library(optiflow_project_warnings INTERFACE)

if(OPTIFLOW_ENABLE_WARNINGS)
    target_compile_options(optiflow_project_warnings INTERFACE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wold-style-cast
            -Woverloaded-virtual
        >
        $<$<CXX_COMPILER_ID:MSVC>:
            /W4
        >
    )
endif()

add_subdirectory(libs/optimization)

if(OPTIFLOW_BUILD_GRPC)
    add_subdirectory(libs/optimizer_proto)
    add_subdirectory(libs/optimizer_service)
endif()

add_subdirectory(apps/solve_cli)

if(OPTIFLOW_BUILD_GRPC)
    add_subdirectory(apps/optimizer_service)
endif()

if(OPTIFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
