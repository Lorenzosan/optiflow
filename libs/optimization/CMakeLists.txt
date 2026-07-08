add_library(optiflow_optimization STATIC
    src/ActionGrid.cpp
    src/GridTypes.cpp
    src/StorageTypes.cpp
    src/Scenario.cpp
    src/BellmanSolver.cpp
    src/CsvScenarioReader.cpp
    src/ForwardSimulator.cpp
    src/Interpolator.cpp
    src/Policy.cpp
    src/PumpedStorageModel.cpp
    src/StateGrid.cpp
    src/ValueFunction.cpp)

target_include_directories(optiflow_optimization
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(optiflow_optimization PUBLIC cxx_std_20)
 
target_link_libraries(optiflow_optimization
    PRIVATE
        optiflow_project_warnings)
