#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>

namespace {

struct CliOptions {
    std::filesystem::path timeseries_path;
    std::filesystem::path constraints_path;
    std::filesystem::path output_path;

    CliOptions(std::filesystem::path timeseries_path,
               std::filesystem::path constraints_path,
               std::filesystem::path output_path)
        : timeseries_path(std::move(timeseries_path)),
          constraints_path(std::move(constraints_path)),
          output_path(std::move(output_path)) {}
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " --timeseries <price_inflow.csv> --constraints <constraints.csv> --output <dispatch.csv>\n"
              << "       " << program_name
              << " --scenario <price_inflow.csv> --constraints <constraints.csv> --output <dispatch.csv>\n";
}

CliOptions parse_args(int argc, char** argv) {
    std::string timeseries_path;
    std::string constraints_path;
    std::string output_path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--timeseries" || arg == "--scenario") && i + 1 < argc) {
            timeseries_path = argv[++i];
        } else if (arg == "--constraints" && i + 1 < argc) {
            constraints_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + arg);
        }
    }

    if (timeseries_path.empty()) {
        throw std::invalid_argument("--timeseries or --scenario is required");
    }
    if (constraints_path.empty()) {
        throw std::invalid_argument("--constraints is required");
    }
    if (output_path.empty()) {
        throw std::invalid_argument("--output is required");
    }

    return CliOptions(timeseries_path, constraints_path, output_path);
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

}  // namespace

int main(int argc, char** argv) {
    try {
        const CliOptions options = parse_args(argc, argv);
        const optiflow::core::ScenarioBundle bundle =
            optiflow::core::CsvScenarioReader::read(options.timeseries_path, options.constraints_path);

        const optiflow::numerics::StateGrid state_grid =
            optiflow::numerics::StateGrid::from_parameters(bundle.scenario.model_parameters(),
                                                           bundle.solver_parameters);
        const optiflow::numerics::ActionGrid action_grid =
            optiflow::numerics::ActionGrid::from_parameters(bundle.scenario.model_parameters(),
                                                            bundle.solver_parameters);
        const optiflow::model::PumpedStorageModel model(bundle.scenario.model_parameters());

        const optiflow::solver::BellmanSolver solver(state_grid,
                                                     action_grid,
                                                     model,
                                                     bundle.solver_parameters);
        const optiflow::solver::BellmanResult result = solver.solve(bundle.scenario);

        const optiflow::solver::ForwardSimulator simulator(state_grid,
                                                           action_grid,
                                                           model,
                                                           bundle.solver_parameters);
        const std::vector<optiflow::core::DispatchStep> trajectory =
            simulator.simulate_from_value_function(bundle.scenario, result.value_function);

        write_dispatch_csv(options.output_path, trajectory);

        const double cumulative_profit = trajectory.empty() ? 0.0 : trajectory.back().cumulative_profit;
        std::cout << "Scenario: " << bundle.scenario.name() << '\n';
        std::cout << "Time steps: " << bundle.scenario.horizon_size() << '\n';
        std::cout << "Actions: " << action_grid.size() << '\n';
        std::cout << "Cumulative profit: " << cumulative_profit << '\n';
        std::cout << "Dispatch written to: " << options.output_path << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
}
