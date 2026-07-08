#include "optiflow/demo/DemoScenario.h"

#include <cstdlib>
#include <exception>
#include <iostream>

int main() {
  try {
    const auto result = optiflow::demo::run_default_dispatch();
    std::cout << optiflow::demo::simulation_to_csv(result);
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
