add_library(optiflow_service_common STATIC
  src/Http.cpp
)

add_library(optiflow::service_common ALIAS optiflow_service_common)

target_include_directories(optiflow_service_common
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(optiflow_service_common PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(optiflow_service_common PRIVATE /W4 /permissive-)
else()
  target_compile_options(optiflow_service_common PRIVATE -Wall -Wextra -Wpedantic)
endif()
