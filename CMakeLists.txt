cmake_minimum_required(VERSION 3.20)

project(OptiFlow
  VERSION 0.1.0
  DESCRIPTION "C++20 pumped-storage and battery dispatch optimizer demo"
  LANGUAGES CXX
)

option(OPTIFLOW_BUILD_CLI "Build the command line demo" ON)
option(OPTIFLOW_BUILD_TESTS "Build the test executables" ON)
option(OPTIFLOW_BUILD_DOCS "Build Doxygen documentation" OFF)
option(OPTIFLOW_BUILD_SERVICES "Build lightweight local HTTP demo services" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(libs/optimization)
add_subdirectory(libs/demo)

if(OPTIFLOW_BUILD_CLI)
  add_subdirectory(apps/optiflow_cli)
endif()

if(OPTIFLOW_BUILD_SERVICES)
  add_subdirectory(libs/service_common)
  add_subdirectory(services/optimizer_service)
  add_subdirectory(services/api_service)
endif()

if(OPTIFLOW_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests/optimization_scenarios)
endif()

if(OPTIFLOW_BUILD_DOCS)
  find_package(Doxygen QUIET)

  if(DOXYGEN_FOUND)
    set(OPTIFLOW_DOXYGEN_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/docs")
    configure_file(
      "${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in"
      "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      @ONLY
    )

    add_custom_target(optiflow_docs
      COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      COMMENT "Generating OptiFlow API documentation with Doxygen"
      VERBATIM
    )
  else()
    add_custom_target(optiflow_docs
      COMMAND "${CMAKE_COMMAND}" -E echo "Doxygen was not found. Install Doxygen to build this target."
      COMMAND "${CMAKE_COMMAND}" -E false
      VERBATIM
    )
  endif()
endif()
