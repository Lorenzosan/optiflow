add_executable(optiflow_solve main.cpp)

target_link_libraries(optiflow_solve PRIVATE optiflow_optimization)

target_compile_features(optiflow_solve PRIVATE cxx_std_20)

target_compile_options(optiflow_solve PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wconversion>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>)
