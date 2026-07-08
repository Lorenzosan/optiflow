add_library(optiflow_optimization
    src/core/StorageTypes.cpp
    src/io/CsvReader.cpp
    src/model/PumpedStorageModel.cpp
    src/model/TerminalPenaltyModel.cpp
    src/numerics/ActionGrid.cpp
    src/numerics/Interpolator.cpp
    src/numerics/Policy.cpp
    src/numerics/StateGrid.cpp
    src/numerics/ValueFunction.cpp
    src/solver/DeterministicProblem.cpp
    src/solver/BellmanEvaluator.cpp
    src/solver/BellmanSolver.cpp
    src/solver/ForwardSimulator.cpp
    src/solver/OptimizationResult.cpp
)

add_library(OptiFlow::optimization ALIAS optiflow_optimization)

target_include_directories(optiflow_optimization
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(optiflow_optimization PUBLIC cxx_std_20)

target_compile_options(optiflow_optimization PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
