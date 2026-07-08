add_library(optiflow_optimization STATIC
  src/numerics/ActionGrid.cpp
  src/numerics/Interpolator.cpp
  src/numerics/Policy.cpp
  src/numerics/StateGrid.cpp
  src/numerics/ValueFunction.cpp
  src/model/PumpedStorageModel.cpp
  src/solver/BellmanSolver.cpp
  src/solver/ForwardSimulator.cpp
)

add_library(optiflow::optimization ALIAS optiflow_optimization)

target_include_directories(optiflow_optimization
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(optiflow_optimization PUBLIC cxx_std_20)

if(MSVC)
  target_compile_options(optiflow_optimization PRIVATE /W4 /permissive-)
else()
  target_compile_options(optiflow_optimization PRIVATE -Wall -Wextra -Wpedantic)
endif()
