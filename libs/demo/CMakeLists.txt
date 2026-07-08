add_library(optiflow_demo STATIC
  src/DemoScenario.cpp
)

add_library(optiflow::demo ALIAS optiflow_demo)

target_include_directories(optiflow_demo
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(optiflow_demo PUBLIC optiflow::optimization)
target_compile_features(optiflow_demo PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(optiflow_demo PRIVATE /W4 /permissive-)
else()
  target_compile_options(optiflow_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()
