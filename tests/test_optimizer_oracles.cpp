#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cmath>
#include <exception>
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

core::TerminalParameters terminal_exact_reservoir(double reservoir_volume,
                                                  double battery_min,
                                                  double battery_max) {
    return core::TerminalParameters(reservoir_volume,
                                    reservoir_volume,
                                    battery_min,
                                    battery_max,
                                    reservoir_volume,
                                    battery_min,
                                    0.0,
                                    0.0);
}

core::ModelParameters hydro_parameters(double reservoir_max,
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

core::ModelParameters battery_parameters(double battery_max_soc,
                                          double charge_max_power,
                                          double discharge_max_power,
                                          double degradation_cost_per_mwh) {
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
                                 degradation_cost_per_mwh,
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

void test_high_price_now_turbines_before_low_price_later() {
    const core::ModelParameters mp = hydro_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::Scenario scenario("high_now_low_later",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(100.0, 0.0), core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    require_near(trajectory.at(0).action.turbine_flow, 10.0, "first-step turbine flow");
    require_near(trajectory.at(0).next_state.reservoir_volume, 0.0, "first-step reservoir volume");
    require_near(trajectory.at(1).action.turbine_flow, 0.0, "second-step turbine flow");
    require_near(trajectory.back().cumulative_profit, 1000.0, "cumulative profit");
}

void test_low_price_now_preserves_water_for_high_price_later() {
    const core::ModelParameters mp = hydro_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::Scenario scenario("low_now_high_later",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(0.0, 0.0), core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    require_near(trajectory.at(0).action.turbine_flow, 0.0, "first-step turbine flow");
    require_near(trajectory.at(0).next_state.reservoir_volume, 10.0, "first-step reservoir volume");
    require_near(trajectory.at(1).action.turbine_flow, 10.0, "second-step turbine flow");
    require_near(trajectory.back().cumulative_profit, 1000.0, "cumulative profit");
}

void test_negative_price_now_pumps_for_high_price_later_when_feasible() {
    const core::ModelParameters mp = hydro_parameters(10.0, 10.0, 10.0);
    const core::SolverParameters sp(2, 1, 2, 1, 2, 1, 1, 1.0);
    const core::Scenario scenario("negative_now_high_later_pump",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(-10.0, 0.0), core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    require_near(trajectory.at(0).action.pump_flow, 10.0, "first-step pump flow");
    require_near(trajectory.at(0).next_state.reservoir_volume, 10.0, "first-step reservoir volume");
    require_near(trajectory.at(1).action.turbine_flow, 10.0, "second-step turbine flow");
    require_near(trajectory.back().cumulative_profit, 1100.0, "cumulative profit");
}

void test_low_degradation_battery_arbitrages_price_spread() {
    const core::ModelParameters mp = battery_parameters(10.0, 10.0, 10.0, 0.0);
    const core::SolverParameters sp(1, 2, 1, 1, 1, 2, 2, 1.0);
    const core::Scenario scenario("low_degradation_battery_arbitrage",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(0.0, 0.0), core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 0.0, 0.0, 10.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    require_near(trajectory.at(0).action.battery_charge_power, 10.0, "first-step battery charge power");
    require_near(trajectory.at(0).next_state.battery_soc, 10.0, "first-step battery state of charge");
    require_near(trajectory.at(1).action.battery_discharge_power, 10.0, "second-step battery discharge power");
    require_near(trajectory.at(1).next_state.battery_soc, 0.0, "final battery state of charge");
    require_near(trajectory.back().cumulative_profit, 1000.0, "cumulative profit");
}

void test_high_degradation_battery_avoids_unprofitable_cycling() {
    const core::ModelParameters mp = battery_parameters(10.0, 10.0, 10.0, 75.0);
    const core::SolverParameters sp(1, 2, 1, 1, 1, 2, 2, 1.0);
    const core::Scenario scenario("high_degradation_battery_no_cycle",
                                  core::State(0.0, 0.0),
                                  {core::Exogenous(0.0, 0.0), core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_open(0.0, 0.0, 0.0, 10.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    require_near(trajectory.at(0).action.battery_charge_power, 0.0, "first-step battery charge power");
    require_near(trajectory.at(0).action.battery_discharge_power, 0.0, "first-step battery discharge power");
    require_near(trajectory.at(1).action.battery_charge_power, 0.0, "second-step battery charge power");
    require_near(trajectory.at(1).action.battery_discharge_power, 0.0, "second-step battery discharge power");
    require_near(trajectory.back().next_state.battery_soc, 0.0, "final battery state of charge");
    require_near(trajectory.back().cumulative_profit, 0.0, "cumulative profit");
}

void test_no_inflow_terminal_reservoir_constraint_preserves_inventory() {
    const core::ModelParameters mp = hydro_parameters(10.0, 10.0, 0.0);
    const core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const core::Scenario scenario("terminal_inventory_no_inflow",
                                  core::State(10.0, 0.0),
                                  {core::Exogenous(100.0, 0.0), core::Exogenous(100.0, 0.0)},
                                  mp,
                                  terminal_exact_reservoir(10.0, 0.0, 0.0));

    const std::vector<core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    require(trajectory.size() == 2, "trajectory length");
    for (const core::DispatchStep& step : trajectory) {
        require_near(step.action.turbine_flow, 0.0, "turbine flow under terminal inventory constraint");
        require_near(step.next_state.reservoir_volume, 10.0, "reservoir volume under terminal inventory constraint");
    }
    require_near(trajectory.back().cumulative_profit, 0.0, "cumulative profit");
}

}  // namespace

int main() {
    test_high_price_now_turbines_before_low_price_later();
    test_low_price_now_preserves_water_for_high_price_later();
    test_negative_price_now_pumps_for_high_price_later_when_feasible();
    test_low_degradation_battery_arbitrages_price_spread();
    test_high_degradation_battery_avoids_unprofitable_cycling();
    test_no_inflow_terminal_reservoir_constraint_preserves_inventory();
    return 0;
}
