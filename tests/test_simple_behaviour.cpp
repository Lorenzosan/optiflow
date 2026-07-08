#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

namespace core = optiflow::core;
namespace model = optiflow::model;
namespace numerics = optiflow::numerics;
namespace solver = optiflow::solver;

constexpr double tolerance = 1.0e-9;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void require_near(double actual, double expected, std::string_view message) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(std::string(message) + ": expected " + std::to_string(expected) +
                                 ", got " + std::to_string(actual));
    }
}

template <typename Exception, typename Function>
void require_throws(Function&& function, std::string_view message_fragment) {
    try {
        function();
    } catch (const Exception& error) {
        if (!message_fragment.empty()) {
            const std::string text(error.what());
            require(text.find(std::string(message_fragment)) != std::string::npos,
                    std::string("exception message did not contain expected text: ") + text);
        }
        return;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("wrong exception type: ") + error.what());
    }

    throw std::runtime_error("expected exception was not thrown");
}

core::TerminalParameters terminal_open(double reservoir_min,
                                       double reservoir_max,
                                       double battery_min,
                                       double battery_max) {
    return core::TerminalParameters(reservoir_min,
                                    reservoir_max,
                                    battery_min,
                                    battery_max,
                                    reservoir_min,
                                    battery_min,
                                    0.0,
                                    0.0);
}

core::ModelParameters hydro_only_parameters(double reservoir_max,
                                            double turbine_max_flow,
                                            double pump_max_flow) {
    return core::ModelParameters(1.0,
                                 0.0,
                                 reservoir_max,
                                 0.0,
                                 0.0,
                                 turbine_max_flow,
                                 pump_max_flow,
                                 0.0,
                                 0.0,
                                 0.0,
                                 1.0,
                                 1.0,
                                 1.0,
                                 1.0,
                                 1.0,
                                 0.0,
                                 0.0,
                                 1000000.0);
}

core::ModelParameters battery_only_parameters(double battery_max_soc,
                                              double charge_max_power,
                                              double discharge_max_power) {
    return core::ModelParameters(1.0,
                                 0.0,
                                 0.0,
                                 0.0,
                                 battery_max_soc,
                                 0.0,
                                 0.0,
                                 0.0,
                                 charge_max_power,
                                 discharge_max_power,
                                 1.0,
                                 1.0,
                                 1.0,
                                 1.0,
                                 1.0,
                                 0.0,
                                 0.0,
                                 1000000.0);
}

std::vector<core::DispatchStep> solve_and_simulate(const core::Scenario& scenario,
                                                   const core::SolverParameters& solver_parameters) {
    const numerics::StateGrid state_grid = numerics::StateGrid::from_parameters(scenario.model_parameters(),
                                                                                solver_parameters);
    const numerics::ActionGrid action_grid = numerics::ActionGrid::from_parameters(scenario.model_parameters(),
                                                                                   solver_parameters);
    const model::PumpedStorageModel pumped_storage_model(scenario.model_parameters());
    const solver::BellmanSolver bellman_solver(state_grid, action_grid, pumped_storage_model, solver_parameters);
    const solver::BellmanResult result = bellman_solver.solve(scenario);
    const solver::ForwardSimulator simulator(state_grid, action_grid, pumped_storage_model, solver_parameters);
    return simulator.simulate_from_value_function(scenario, result.value_function);
}

void test_zero_price_waits_when_no_terminal_requirement() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::Scenario scenario("zero_price_wait",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.turbine_flow, 0.0, "turbine flow");
    require_near(trajectory.front().action.pump_flow, 0.0, "pump flow");
    require_near(trajectory.front().next_state.reservoir_volume, 10.0, "next reservoir volume");
    require_near(trajectory.front().reward, 0.0, "reward");
}

void test_single_step_high_price_turbines_available_water() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::Scenario scenario("high_price_turbine",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.turbine_flow, 10.0, "turbine flow");
    require_near(trajectory.front().next_state.reservoir_volume, 0.0, "next reservoir volume");
    require_near(trajectory.front().net_power, 10.0, "net power");
    require_near(trajectory.front().reward, 1000.0, "reward");
}

void test_hard_terminal_reservoir_bound_forces_wait() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const core::Scenario scenario("hard_terminal_wait",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal);

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.turbine_flow, 0.0, "turbine flow");
    require_near(trajectory.front().next_state.reservoir_volume, 10.0, "next reservoir volume");
    require_near(trajectory.front().reward, 0.0, "reward");
}

void test_soft_terminal_reservoir_penalty_can_prevent_turbining() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::TerminalParameters terminal(0.0, 10.0, 0.0, 0.0, 10.0, 0.0, 20.0, 0.0);
    const core::Scenario scenario("soft_terminal_wait",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal);

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.turbine_flow, 0.0, "turbine flow");
    require_near(trajectory.front().next_state.reservoir_volume, 10.0, "next reservoir volume");
}

void test_hard_terminal_reservoir_bound_forces_pumping() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 0.0, 10.0);
    const core::SolverParameters sp(2, 1, 1, 1, 2, 1, 1, 1.0);
    const core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const core::Scenario scenario("hard_terminal_pump",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal);

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.pump_flow, 10.0, "pump flow");
    require_near(trajectory.front().next_state.reservoir_volume, 10.0, "next reservoir volume");
    require_near(trajectory.front().net_power, -10.0, "net power");
}

void test_single_step_high_price_discharges_battery() {
    const core::ModelParameters mp = battery_only_parameters(10.0, 0.0, 10.0);
    const core::SolverParameters sp(1, 2, 1, 1, 1, 1, 2, 1.0);
    const core::Scenario scenario("high_price_discharge",
                                  core::State(0.0, 10.0),
                                  {core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 0.0, 0.0, 10.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.battery_discharge_power, 10.0, "battery discharge power");
    require_near(trajectory.front().next_state.battery_soc, 0.0, "next battery state of charge");
    require_near(trajectory.front().net_power, 10.0, "net power");
    require_near(trajectory.front().reward, 1000.0, "reward");
}

void test_hard_terminal_battery_bound_forces_charging() {
    const core::ModelParameters mp = battery_only_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(1, 2, 1, 1, 1, 2, 1, 1.0);
    const core::TerminalParameters terminal(0.0, 0.0, 10.0, 10.0, 0.0, 10.0, 0.0, 0.0);
    const core::Scenario scenario("hard_terminal_charge",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal);

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 1, "trajectory length");
    require_near(trajectory.front().action.battery_charge_power, 10.0, "battery charge power");
    require_near(trajectory.front().next_state.battery_soc, 10.0, "next battery state of charge");
    require_near(trajectory.front().net_power, -10.0, "net power");
}

void test_forward_simulation_throws_when_terminal_bound_is_unreachable() {
    const core::ModelParameters mp = hydro_only_parameters(10.0, 0.0, 0.0);
    const core::SolverParameters sp(2, 1, 1, 1, 1, 1, 1, 1.0);
    const core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const core::Scenario scenario("unreachable_terminal",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal);

    require_throws<std::runtime_error>([&]() {
        const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);
        static_cast<void>(trajectory);
    }, "no feasible action");
}

void test_csv_reader_accepts_complete_minimal_files() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::filesystem::path timeseries_path = directory / "optiflow_simple_valid_timeseries.csv";
    const std::filesystem::path constraints_path = directory / "optiflow_simple_valid_constraints.csv";

    {
        std::ofstream timeseries(timeseries_path);
        timeseries << "time_index,price,natural_inflow\n";
        timeseries << "0,100,0\n";
    }
    {
        std::ofstream constraints(constraints_path);
        constraints << "key,value\n";
        constraints << "scenario_name,minimal_valid\n";
        constraints << "time_step_hours,1\n";
        constraints << "reservoir_min_volume,0\n";
        constraints << "reservoir_max_volume,10\n";
        constraints << "battery_min_soc,0\n";
        constraints << "battery_max_soc,0\n";
        constraints << "turbine_max_flow,10\n";
        constraints << "pump_max_flow,0\n";
        constraints << "spill_max_flow,0\n";
        constraints << "battery_max_charge_power,0\n";
        constraints << "battery_max_discharge_power,0\n";
        constraints << "turbine_efficiency,1\n";
        constraints << "pump_efficiency,1\n";
        constraints << "battery_charge_efficiency,1\n";
        constraints << "battery_discharge_efficiency,1\n";
        constraints << "water_to_power_factor,1\n";
        constraints << "battery_degradation_cost_per_mwh,0\n";
        constraints << "operating_cost_per_mwh,0\n";
        constraints << "infeasibility_penalty,1000000\n";
        constraints << "terminal_reservoir_min_volume,0\n";
        constraints << "terminal_reservoir_max_volume,10\n";
        constraints << "terminal_battery_min_soc,0\n";
        constraints << "terminal_battery_max_soc,0\n";
        constraints << "terminal_target_reservoir_volume,0\n";
        constraints << "terminal_target_battery_soc,0\n";
        constraints << "terminal_reservoir_target_penalty,0\n";
        constraints << "terminal_battery_target_penalty,0\n";
        constraints << "initial_reservoir_volume,10\n";
        constraints << "initial_battery_soc,0\n";
        constraints << "reservoir_volume_grid_points,2\n";
        constraints << "battery_soc_grid_points,1\n";
        constraints << "turbine_flow_steps,2\n";
        constraints << "spill_flow_steps,1\n";
        constraints << "pump_flow_steps,1\n";
        constraints << "battery_charge_steps,1\n";
        constraints << "battery_discharge_steps,1\n";
        constraints << "discount_factor,1\n";
    }

    const core::ScenarioBundle bundle = core::CsvScenarioReader::read(timeseries_path, constraints_path);

    std::filesystem::remove(timeseries_path);
    std::filesystem::remove(constraints_path);

    require(bundle.scenario.name() == "minimal_valid", "scenario name");
    require(bundle.scenario.horizon_size() == 1, "horizon length");
    require_near(bundle.scenario.initial_state().reservoir_volume, 10.0, "initial reservoir volume");
    require(bundle.solver_parameters.reservoir_volume_grid_points == 2, "reservoir grid points");
}

}  // namespace

int main() {
    test_zero_price_waits_when_no_terminal_requirement();
    test_single_step_high_price_turbines_available_water();
    test_hard_terminal_reservoir_bound_forces_wait();
    test_soft_terminal_reservoir_penalty_can_prevent_turbining();
    test_hard_terminal_reservoir_bound_forces_pumping();
    test_single_step_high_price_discharges_battery();
    test_hard_terminal_battery_bound_forces_charging();
    test_forward_simulation_throws_when_terminal_bound_is_unreachable();
    test_csv_reader_accepts_complete_minimal_files();
    return 0;
}
