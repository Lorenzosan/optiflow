#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/runner/OptimizationRunner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CliOptions {
    std::filesystem::path scenario_path;
    std::filesystem::path prices_path;
    std::filesystem::path inflows_path;
    std::filesystem::path output_path;

    CliOptions(std::filesystem::path scenario_path_arg,
               std::filesystem::path prices_path_arg,
               std::filesystem::path inflows_path_arg,
               std::filesystem::path output_path_arg)
        : scenario_path(std::move(scenario_path_arg)),
          prices_path(std::move(prices_path_arg)),
          inflows_path(std::move(inflows_path_arg)),
          output_path(std::move(output_path_arg)) {}
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " --scenario <scenario.csv> --prices <prices.csv> --inflows <inflows.csv> "
              << "--output <dispatch.csv>\n\n"
              << "Inputs:\n"
              << "  --scenario   CSV file with key,value rows for scenario, model, terminal, and solver parameters.\n"
              << "  --prices     CSV file with time_index,price rows.\n"
              << "  --inflows    CSV file with time_index,natural_inflow rows.\n"
              << "  --output     Dispatch CSV output path.\n";
}

CliOptions parse_args(int argc, char** argv) {
    std::string scenario_path;
    std::string prices_path;
    std::string inflows_path;
    std::string output_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--scenario" && i + 1 < argc) {
            scenario_path = argv[++i];
        } else if (arg == "--prices" && i + 1 < argc) {
            prices_path = argv[++i];
        } else if (arg == "--inflows" && i + 1 < argc) {
            inflows_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + arg);
        }
    }

    if (scenario_path.empty()) {
        throw std::invalid_argument("--scenario is required");
    }
    if (prices_path.empty()) {
        throw std::invalid_argument("--prices is required");
    }
    if (inflows_path.empty()) {
        throw std::invalid_argument("--inflows is required");
    }
    if (output_path.empty()) {
        throw std::invalid_argument("--output is required");
    }

    return CliOptions(scenario_path, prices_path, inflows_path, output_path);
}

void write_dispatch_csv(const std::filesystem::path& output_path,
                        const std::vector<optiflow::core::DispatchStep>& trajectory) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + output_path.string());
    }

    output << "time_index,price,natural_inflow,reservoir_volume,battery_soc,"
           << "turbine_flow,spill_flow,pump_flow,battery_charge_power,battery_discharge_power,"
           << "next_reservoir_volume,next_battery_soc,net_power,reward,cumulative_profit\n";

    for (const optiflow::core::DispatchStep& step : trajectory) {
        output << step.time_index << ','
               << step.exogenous.electricity_price << ','
               << step.exogenous.natural_inflow << ','
               << step.state.reservoir_volume << ','
               << step.state.battery_soc << ','
               << step.action.turbine_flow << ','
               << step.action.spill_flow << ','
               << step.action.pump_flow << ','
               << step.action.battery_charge_power << ','
               << step.action.battery_discharge_power << ','
               << step.next_state.reservoir_volume << ','
               << step.next_state.battery_soc << ','
               << step.net_power << ','
               << step.reward << ','
               << step.cumulative_profit << '\n';
    }
}

void print_summary(std::ostream& output,
                   const optiflow::core::ScenarioBundle& bundle,
                   const optiflow::runner::OptimizationResult& result,
                   const std::filesystem::path& dispatch_path) {
    const optiflow::runner::OptimizationDiagnostics& diagnostics = result.diagnostics;

    output << "Scenario: " << bundle.scenario.name() << '\n';
    output << "Time steps: " << diagnostics.horizon_steps << '\n';
    output << "Reservoir grid points: " << diagnostics.reservoir_grid_points << '\n';
    output << "Battery grid points: " << diagnostics.battery_grid_points << '\n';
    output << "Action count: " << diagnostics.action_count << '\n';
    output << "Solve seconds: " << diagnostics.solve_seconds << '\n';
    output << "Simulation seconds: " << diagnostics.simulation_seconds << '\n';
    output << "Turbine steps: " << diagnostics.turbine_steps << '\n';
    output << "Pump steps: " << diagnostics.pump_steps << '\n';
    output << "Spill steps: " << diagnostics.spill_steps << '\n';
    output << "Battery charge steps: " << diagnostics.battery_charge_steps << '\n';
    output << "Battery discharge steps: " << diagnostics.battery_discharge_steps << '\n';
    output << "Wait steps: " << diagnostics.wait_steps << '\n';
    output << "Cumulative profit: " << result.cumulative_profit << '\n';
    output << "Dispatch written to: " << dispatch_path << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_args(argc, argv);
        const optiflow::core::ScenarioBundle bundle =
            optiflow::core::CsvScenarioReader::read(options.scenario_path,
                                                    options.prices_path,
                                                    options.inflows_path);

        const optiflow::runner::OptimizationRunner runner;
        const optiflow::runner::OptimizationResult result = runner.run(bundle);

        write_dispatch_csv(options.output_path, result.dispatch);
        print_summary(std::cout, bundle, result, options.output_path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
}
