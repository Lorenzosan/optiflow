#include "optiflow/demo/CsvScenarioLoader.h"
#include "optiflow/demo/DemoScenario.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void print_usage(std::ostream& stream, const char* program_name) {
  stream << "usage:\n"
         << "  " << program_name << "\n"
         << "  " << program_name << " --prices <price_csv> --inflows <inflow_csv>\n\n"
         << "price CSV header:  time_index,price_eur_per_mwh\n"
         << "inflow CSV header: time_index,natural_inflow_m3_s\n";
}

}  // namespace

int main(const int argc, char* argv[]) {
  try {
    std::string price_csv_path;
    std::string inflow_csv_path;

    for (int index = 1; index < argc; ++index) {
      const std::string argument{argv[index]};

      if (argument == "--help" || argument == "-h") {
        print_usage(std::cout, argv[0]);
        return EXIT_SUCCESS;
      }

      if (argument == "--prices") {
        if (index + 1 >= argc) {
          throw std::invalid_argument{"--prices requires a CSV path"};
        }
        price_csv_path = argv[++index];
        continue;
      }

      if (argument == "--inflows") {
        if (index + 1 >= argc) {
          throw std::invalid_argument{"--inflows requires a CSV path"};
        }
        inflow_csv_path = argv[++index];
        continue;
      }

      throw std::invalid_argument{"unknown argument: " + argument};
    }

    const auto has_price_csv = !price_csv_path.empty();
    const auto has_inflow_csv = !inflow_csv_path.empty();
    if (has_price_csv != has_inflow_csv) {
      throw std::invalid_argument{"--prices and --inflows must be provided together"};
    }

    if (!has_price_csv) {
      std::cout << optiflow::demo::simulation_to_csv(optiflow::demo::run_default_dispatch());
      return EXIT_SUCCESS;
    }

    const auto exogenous = optiflow::demo::load_deterministic_exogenous_csv(
        price_csv_path,
        inflow_csv_path);
    std::cout << optiflow::demo::simulation_to_csv(optiflow::demo::run_dispatch(exogenous));
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
