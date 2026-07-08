add_executable(optiflow_solve main.cpp)

target_link_libraries(optiflow_solve
    PRIVATE
        optiflow_optimization
        optiflow_project_warnings)

target_compile_features(optiflow_solve PRIVATE cxx_std_20)
