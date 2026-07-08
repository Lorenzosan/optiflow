#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <optiflow/io/CsvReader.hpp>
#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

namespace {

void print_usage() {
    std::cout << "Usage: optiflow_cli --prices PATH --inflows PATH [constraints]\n\n"
              << "Required:\n"
              << "  --prices PATH\n"
              << "  --inflows PATH\n\n"
              << "Constraints and configuration:\n"
              << "  --initial-reservoir-volume VALUE\n"
              << "  --min-reservoir-volume VALUE\n"
              << "  --max-reservoir-volume VALUE\n"
              << "  --max-turbine-flow VALUE\n"
              << "  --max-pump-flow VALUE\n"
              << "  --hydraulic-head VALUE\n"
              << "  --turbine-efficiency VALUE\n"
              << "  --pump-efficiency VALUE\n"
              << "  --timestep-hours VALUE\n"
              << "  --discount-factor VALUE\n"
              << "  --target-final-reservoir-volume VALUE\n"
              << "  --terminal-reservoir-penalty VALUE\n"
              << "  --overflow-spill-penalty VALUE\n"
              << "  --volume-grid-points VALUE\n"
              << "  --turbine-flow-steps VALUE\n"
              << "  --pump-flow-steps VALUE\n";
}

std::unordered_map<std::string, std::string> parse_args(int argc, char** argv) {
    std::unordered_map<std::string, std::string> args;
    for (int index = 1; index < argc; ++index) {
        const std::string key = argv[index];
        if (key == "--help" || key == "-h") {
            args[key] = "";
            continue;
        }
        if (!key.starts_with("--")) {
            throw std::invalid_argument("unexpected positional argument: " + key);
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("missing value for argument: " + key);
        }
        args[key] = argv[++index];
    }
    return args;
}

double get_double(const std::unordered_map<std::string, std::string>& args,
                  const std::string& key,
                  double default_value) {
    const auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return std::stod(it->second);
}

std::size_t get_size(const std::unordered_map<std::string, std::string>& args,
                     const std::string& key,
                     std::size_t default_value) {
    const auto it = args.find(key);
    if (it == args.end()) {
        return default_value;
    }
    return static_cast<std::size_t>(std::stoull(it->second));
}

std::filesystem::path get_required_path(const std::unordered_map<std::string, std::string>& args,
                                        const std::string& key) {
    const auto it = args.find(key);
    if (it == args.end()) {
        throw std::invalid_argument("missing required argument: " + key);
    }
    return std::filesystem::path(it->second);
}

std::string mode_name(optiflow::HydroMode mode) {
    switch (mode) {
    case optiflow::HydroMode::Idle:
        return "idle";
    case optiflow::HydroMode::Turbine:
        return "turbine";
    case optiflow::HydroMode::Pump:
        return "pump";
    }
    return "unknown";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto args = parse_args(argc, argv);
        if (args.contains("--help") || args.contains("-h")) {
            print_usage();
            return EXIT_SUCCESS;
        }

        const auto price_path = get_required_path(args, "--prices");
        const auto inflow_path = get_required_path(args, "--inflows");

        optiflow::DeterministicProblem problem;
        problem.exogenous = optiflow::read_deterministic_series(price_path, inflow_path);

        problem.reservoir.initial_volume_m3 =
            get_double(args, "--initial-reservoir-volume", 500000.0);
        problem.reservoir.min_volume_m3 = get_double(args, "--min-reservoir-volume", 100000.0);
        problem.reservoir.max_volume_m3 = get_double(args, "--max-reservoir-volume", 1000000.0);
        problem.reservoir.max_turbine_flow_m3_s = get_double(args, "--max-turbine-flow", 80.0);
        problem.reservoir.max_pump_flow_m3_s = get_double(args, "--max-pump-flow", 60.0);

        problem.hydro.hydraulic_head_m = get_double(args, "--hydraulic-head", 300.0);
        problem.hydro.turbine_efficiency = get_double(args, "--turbine-efficiency", 0.90);
        problem.hydro.pump_efficiency = get_double(args, "--pump-efficiency", 0.85);
        problem.hydro.timestep_hours = get_double(args, "--timestep-hours", 1.0);

        problem.economics.discount_factor = get_double(args, "--discount-factor", 1.0);
        problem.economics.overflow_spill_penalty_eur_per_m3 =
            get_double(args, "--overflow-spill-penalty", 0.01);

        problem.terminal_reservoir.target_volume_m3 =
            get_double(args, "--target-final-reservoir-volume",
                       problem.reservoir.initial_volume_m3);
        problem.terminal_reservoir.penalty_eur_per_m3 =
            get_double(args, "--terminal-reservoir-penalty", 0.10);

        problem.solver.volume_grid_points = get_size(args, "--volume-grid-points", 101);
        problem.solver.turbine_flow_steps = get_size(args, "--turbine-flow-steps", 8);
        problem.solver.pump_flow_steps = get_size(args, "--pump-flow-steps", 6);

        const optiflow::BellmanSolver solver;
        const auto result = solver.solve(problem);
        const auto dispatch = optiflow::ForwardSimulator::simulate(result);

        double total_reward = 0.0;
        double total_turbine_mwh = 0.0;
        double total_pump_mwh = 0.0;
        double total_spill_m3 = 0.0;
        for (const auto& step : dispatch) {
            total_reward += step.reward_eur;
            total_turbine_mwh += step.turbine_power_mw * problem.hydro.timestep_hours;
            total_pump_mwh += step.pump_power_mw * problem.hydro.timestep_hours;
            total_spill_m3 += step.overflow_spill_m3;
        }

        const double final_reservoir_m3 = dispatch.empty()
                                             ? problem.reservoir.initial_volume_m3
                                             : dispatch.back().reservoir_end_m3;
        const double final_reservoir_deviation_m3 =
            std::abs(final_reservoir_m3 - problem.terminal_reservoir.target_volume_m3);
        const double terminal_reservoir_penalty_eur =
            final_reservoir_deviation_m3 * problem.terminal_reservoir.penalty_eur_per_m3;
        const double forward_total_value_eur = total_reward - terminal_reservoir_penalty_eur;
        const double value_realization_gap_eur = result.objective_value_eur - forward_total_value_eur;
        const double value_realization_gap_pct =
            result.objective_value_eur == 0.0
                ? 0.0
                : 100.0 * value_realization_gap_eur / std::abs(result.objective_value_eur);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "objective_value_eur," << result.objective_value_eur << "\n";
        std::cout << "forward_reward_eur," << total_reward << "\n";
        std::cout << "terminal_reservoir_penalty_eur," << terminal_reservoir_penalty_eur << "\n";
        std::cout << "forward_total_value_eur," << forward_total_value_eur << "\n";
        std::cout << "value_realization_gap_eur," << value_realization_gap_eur << "\n";
        std::cout << "value_realization_gap_pct," << value_realization_gap_pct << "\n";
        std::cout << "state_grid_points," << result.state_grid.size() << "\n";
        std::cout << "final_reservoir_m3," << final_reservoir_m3 << "\n";
        std::cout << "target_final_reservoir_m3,"
                  << problem.terminal_reservoir.target_volume_m3 << "\n";
        std::cout << "final_reservoir_deviation_m3," << final_reservoir_deviation_m3 << "\n";
        std::cout << "total_turbine_mwh," << total_turbine_mwh << "\n";
        std::cout << "total_pump_mwh," << total_pump_mwh << "\n";
        std::cout << "total_overflow_spill_m3," << total_spill_m3 << "\n\n";

        std::cout << "time_index,price_eur_per_mwh,inflow_m3_s,reservoir_start_m3,"
                  << "reservoir_end_m3,mode,turbine_flow_m3_s,pump_flow_m3_s,"
                  << "turbine_power_mw,pump_power_mw,net_power_mw,overflow_spill_m3,reward_eur\n";

        for (const auto& step : dispatch) {
            std::cout << step.time_index << ',' << step.price_eur_per_mwh << ','
                      << step.natural_inflow_m3_s << ',' << step.reservoir_start_m3 << ','
                      << step.reservoir_end_m3 << ',' << mode_name(step.action.mode) << ','
                      << step.action.turbine_flow_m3_s << ',' << step.action.pump_flow_m3_s << ','
                      << step.turbine_power_mw << ',' << step.pump_power_mw << ','
                      << step.net_power_mw << ',' << step.overflow_spill_m3 << ','
                      << step.reward_eur << '\n';
        }

        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        std::cerr << "Use --help for usage.\n";
        return EXIT_FAILURE;
    }
}
