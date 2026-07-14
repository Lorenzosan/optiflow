#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/runner/OptimizationRunner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
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
    std::filesystem::path summary_output_path;
    bool validate_only;
};

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name
              << " --scenario <scenario.csv> --prices <prices.csv> --inflows <inflows.csv> "
              << "(--output <dispatch.csv> [--summary-output <summary.json>] | --validate-only)\n\n"
              << "Inputs:\n"
              << "  --scenario        CSV file with key,value rows for scenario, model, terminal, and solver parameters.\n"
              << "  --prices          CSV file with timestamp_utc,price rows.\n"
              << "  --inflows         CSV file with timestamp_utc,natural_inflow rows.\n"
              << "  --output          Dispatch CSV output path. Required unless --validate-only is used.\n"
              << "  --summary-output  Optional machine-readable run summary JSON path.\n"
              << "  --validate-only   Parse and validate all inputs without solving or writing output files.\n";
}

CliOptions parse_args(int argc, char** argv) {
    CliOptions options{};
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--scenario" && index + 1 < argc) {
            options.scenario_path = argv[++index];
        } else if (argument == "--prices" && index + 1 < argc) {
            options.prices_path = argv[++index];
        } else if (argument == "--inflows" && index + 1 < argc) {
            options.inflows_path = argv[++index];
        } else if (argument == "--output" && index + 1 < argc) {
            options.output_path = argv[++index];
        } else if (argument == "--summary-output" && index + 1 < argc) {
            options.summary_output_path = argv[++index];
        } else if (argument == "--validate-only") {
            options.validate_only = true;
        } else if (argument == "--help" || argument == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::invalid_argument("unknown or incomplete argument: " + argument);
        }
    }

    if (options.scenario_path.empty()) {
        throw std::invalid_argument("--scenario is required");
    }
    if (options.prices_path.empty()) {
        throw std::invalid_argument("--prices is required");
    }
    if (options.inflows_path.empty()) {
        throw std::invalid_argument("--inflows is required");
    }
    if (!options.validate_only && options.output_path.empty()) {
        throw std::invalid_argument("--output is required unless --validate-only is used");
    }
    if (options.validate_only && !options.output_path.empty()) {
        throw std::invalid_argument("--output cannot be used with --validate-only");
    }
    if (options.validate_only && !options.summary_output_path.empty()) {
        throw std::invalid_argument("--summary-output cannot be used with --validate-only");
    }
    return options;
}

void write_dispatch_csv(const std::filesystem::path& output_path,
                        const std::vector<optiflow::core::DispatchStep>& trajectory) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + output_path.string());
    }

    output << "time_index,timestamp_utc,price,natural_inflow,reservoir_volume,"
           << "turbine_flow,spill_flow,pump_flow,next_reservoir_volume,"
           << "net_power,reward,cumulative_profit\n";
    for (const optiflow::core::DispatchStep& step : trajectory) {
        output << step.time_index << ','
               << step.exogenous.timestamp_utc << ','
               << step.exogenous.electricity_price << ','
               << step.exogenous.natural_inflow << ','
               << step.state.reservoir_volume << ','
               << step.action.turbine_flow << ','
               << step.action.spill_flow << ','
               << step.action.pump_flow << ','
               << step.next_state.reservoir_volume << ','
               << step.net_power << ','
               << step.reward << ','
               << step.cumulative_profit << '\n';
    }
}

void write_summary_json(const std::filesystem::path& output_path,
                        const optiflow::runner::OptimizationResult& result) {
    std::ofstream output(output_path);
    if (!output) {
        throw std::runtime_error("cannot open summary output file: " + output_path.string());
    }
    const optiflow::runner::OptimizationDiagnostics& diagnostics = result.diagnostics;
    output << std::setprecision(17)
           << "{\n"
           << "  \"cumulative_profit\": " << result.cumulative_profit << ",\n"
           << "  \"export_energy_mwh\": " << diagnostics.export_energy_mwh << ",\n"
           << "  \"import_energy_mwh\": " << diagnostics.import_energy_mwh << ",\n"
           << "  \"final_reservoir_volume\": " << diagnostics.final_reservoir_volume << ",\n"
           << "  \"solve_seconds\": " << diagnostics.solve_seconds << ",\n"
           << "  \"simulation_seconds\": " << diagnostics.simulation_seconds << ",\n"
           << "  \"turbine_steps\": " << diagnostics.turbine_steps << ",\n"
           << "  \"pump_steps\": " << diagnostics.pump_steps << ",\n"
           << "  \"spill_steps\": " << diagnostics.spill_steps << ",\n"
           << "  \"wait_steps\": " << diagnostics.wait_steps << "\n"
           << "}\n";
}

void print_summary(std::ostream& output,
                   const optiflow::core::ScenarioBundle& bundle,
                   const optiflow::runner::OptimizationResult& result,
                   const std::filesystem::path& dispatch_path) {
    const optiflow::runner::OptimizationDiagnostics& diagnostics = result.diagnostics;
    output << "Scenario: " << bundle.scenario.name() << '\n';
    output << "Time steps: " << diagnostics.horizon_steps << '\n';
    output << "Reservoir grid points: " << diagnostics.reservoir_grid_points << '\n';
    output << "Action count: " << diagnostics.action_count << '\n';
    output << "Solve seconds: " << diagnostics.solve_seconds << '\n';
    output << "Simulation seconds: " << diagnostics.simulation_seconds << '\n';
    output << "Export energy [MWh]: " << diagnostics.export_energy_mwh << '\n';
    output << "Import energy [MWh]: " << diagnostics.import_energy_mwh << '\n';
    output << "Final reservoir content [MWh hydraulic]: " << diagnostics.final_reservoir_volume << '\n';
    output << "Turbine steps: " << diagnostics.turbine_steps << '\n';
    output << "Pump steps: " << diagnostics.pump_steps << '\n';
    output << "Spill steps: " << diagnostics.spill_steps << '\n';
    output << "Wait steps: " << diagnostics.wait_steps << '\n';
    output << "Cumulative profit [€]: " << result.cumulative_profit << '\n';
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
        if (options.validate_only) {
            std::cout << "Scenario valid: " << bundle.scenario.name() << '\n';
            return 0;
        }

        const optiflow::runner::OptimizationResult result =
            optiflow::runner::OptimizationRunner().run(bundle);
        write_dispatch_csv(options.output_path, result.dispatch);
        if (!options.summary_output_path.empty()) {
            write_summary_json(options.summary_output_path, result);
        }
        print_summary(std::cout, bundle, result, options.output_path);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        print_usage(argv[0]);
        return 1;
    }
}
