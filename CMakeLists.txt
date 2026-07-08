cmake_minimum_required(VERSION 3.20)

project(OptiFlow VERSION 0.1.0 LANGUAGES CXX)

option(OPTIFLOW_BUILD_CLI "Build the OptiFlow CLI" ON)
option(OPTIFLOW_BUILD_TESTS "Build OptiFlow unit tests" ON)
option(OPTIFLOW_BUILD_DOCS "Build Doxygen documentation" OFF)

add_subdirectory(libs/optimization)

if(OPTIFLOW_BUILD_CLI)
    add_subdirectory(apps/optiflow_cli)
endif()

if(OPTIFLOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OPTIFLOW_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in
                   ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(optiflow_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating OptiFlow API documentation")
endif()
