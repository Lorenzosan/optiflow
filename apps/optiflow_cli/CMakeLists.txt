add_executable(optiflow_cli main.cpp)

target_link_libraries(optiflow_cli PRIVATE OptiFlow::optimization)

target_compile_features(optiflow_cli PRIVATE cxx_std_20)

target_compile_options(optiflow_cli PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
)
