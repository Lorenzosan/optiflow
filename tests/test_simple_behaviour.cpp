#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool near(double lhs, double rhs) {
    return std::abs(lhs - rhs) < 1.0e-9;
}

optiflow::core::TerminalParameters terminal_open(double reservoir_min,
                                                 double reservoir_max,
                                                 double battery_min,
                                                 double battery_max) {
    return optiflow::core::TerminalParameters(reservoir_min,
                                              reservoir_max,
                                              battery_min,
                                              battery_max,
                                              reservoir_min,
                                              battery_min,
                                              0.0,
                                              0.0);
}

std::vector<optiflow::core::DispatchStep> solve_and_simulate(
    const optiflow::core::Scenario& scenario,
    const optiflow::core::SolverParameters& solver_parameters) {
    const optiflow::numerics::StateGrid state_grid =
        optiflow::numerics::StateGrid::from_parameters(scenario.model_parameters(), solver_parameters);
    const optiflow::numerics::ActionGrid action_grid =
        optiflow::numerics::ActionGrid::from_parameters(scenario.model_parameters(), solver_parameters);
    const optiflow::model::PumpedStorageModel model(scenario.model_parameters());
    const optiflow::solver::BellmanSolver solver(state_grid, action_grid, model, solver_parameters);
    const optiflow::solver::BellmanResult result = solver.solve(scenario);
    const optiflow::solver::ForwardSimulator simulator(state_grid, action_grid, model, solver_parameters);
    return simulator.simulate_from_value_function(scenario, result.value_function);
}

optiflow::core::ModelParameters hydro_only_parameters(double reservoir_max,
                                                       double turbine_max_flow,
                                                       double pump_max_flow) {
    return optiflow::core::ModelParameters(1.0,
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

optiflow::core::ModelParameters battery_only_parameters(double battery_max_soc,
                                                         double charge_max_power,
                                                         double discharge_max_power) {
    return optiflow::core::ModelParameters(1.0,
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

void test_zero_price_waits_when_no_terminal_requirement() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const optiflow::core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const optiflow::core::Scenario scenario("zero_price_wait",
                                            optiflow::core::State(10.0, 0.0),
                                            {optiflow::core::Exogenous(0.0, 0.0)},
                                            mp,
                                            terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.turbine_flow, 0.0));
    assert(near(trajectory.front().next_state.reservoir_volume, 10.0));
    assert(near(trajectory.front().reward, 0.0));
}

void test_single_step_high_price_turbines_available_water() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const optiflow::core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const optiflow::core::Scenario scenario("high_price_turbine",
                                            optiflow::core::State(10.0, 0.0),
                                            {optiflow::core::Exogenous(100.0, 0.0)},
                                            mp,
                                            terminal_open(0.0, 10.0, 0.0, 0.0));

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.turbine_flow, 10.0));
    assert(near(trajectory.front().next_state.reservoir_volume, 0.0));
    assert(near(trajectory.front().net_power, 10.0));
    assert(near(trajectory.front().reward, 1000.0));
}

void test_hard_terminal_reservoir_bound_forces_wait() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const optiflow::core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const optiflow::core::Scenario scenario("hard_terminal_wait",
                                            optiflow::core::State(10.0, 0.0),
                                            {optiflow::core::Exogenous(100.0, 0.0)},
                                            mp,
                                            terminal);

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.turbine_flow, 0.0));
    assert(near(trajectory.front().next_state.reservoir_volume, 10.0));
    assert(near(trajectory.front().reward, 0.0));
}

void test_soft_terminal_reservoir_penalty_can_prevent_turbining() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 10.0, 0.0);
    const optiflow::core::SolverParameters sp(2, 1, 2, 1, 1, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(0.0, 10.0, 0.0, 0.0, 10.0, 0.0, 20.0, 0.0);
    const optiflow::core::Scenario scenario("soft_terminal_wait",
                                            optiflow::core::State(10.0, 0.0),
                                            {optiflow::core::Exogenous(100.0, 0.0)},
                                            mp,
                                            terminal);

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.turbine_flow, 0.0));
    assert(near(trajectory.front().next_state.reservoir_volume, 10.0));
}

void test_hard_terminal_reservoir_bound_forces_pumping() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 0.0, 10.0);
    const optiflow::core::SolverParameters sp(2, 1, 1, 1, 2, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const optiflow::core::Scenario scenario("hard_terminal_pump",
                                            optiflow::core::State(0.0, 0.0),
                                            {optiflow::core::Exogenous(0.0, 0.0)},
                                            mp,
                                            terminal);

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.pump_flow, 10.0));
    assert(near(trajectory.front().next_state.reservoir_volume, 10.0));
    assert(near(trajectory.front().net_power, -10.0));
}

void test_single_step_high_price_discharges_battery() {
    const optiflow::core::ModelParameters mp = battery_only_parameters(10.0, 0.0, 10.0);
    const optiflow::core::SolverParameters sp(1, 2, 1, 1, 1, 1, 2, 1.0);
    const optiflow::core::Scenario scenario("high_price_discharge",
                                            optiflow::core::State(0.0, 10.0),
                                            {optiflow::core::Exogenous(100.0, 0.0)},
                                            mp,
                                            terminal_open(0.0, 0.0, 0.0, 10.0));

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.battery_discharge_power, 10.0));
    assert(near(trajectory.front().next_state.battery_soc, 0.0));
    assert(near(trajectory.front().net_power, 10.0));
    assert(near(trajectory.front().reward, 1000.0));
}

void test_hard_terminal_battery_bound_forces_charging() {
    const optiflow::core::ModelParameters mp = battery_only_parameters(10.0, 10.0, 0.0);
    const optiflow::core::SolverParameters sp(1, 2, 1, 1, 1, 2, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(0.0, 0.0, 10.0, 10.0, 0.0, 10.0, 0.0, 0.0);
    const optiflow::core::Scenario scenario("hard_terminal_charge",
                                            optiflow::core::State(0.0, 0.0),
                                            {optiflow::core::Exogenous(0.0, 0.0)},
                                            mp,
                                            terminal);

    const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().action.battery_charge_power, 10.0));
    assert(near(trajectory.front().next_state.battery_soc, 10.0));
    assert(near(trajectory.front().net_power, -10.0));
}

void test_forward_simulation_throws_when_terminal_bound_is_unreachable() {
    const optiflow::core::ModelParameters mp = hydro_only_parameters(10.0, 0.0, 0.0);
    const optiflow::core::SolverParameters sp(2, 1, 1, 1, 1, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(10.0, 10.0, 0.0, 0.0, 10.0, 0.0, 0.0, 0.0);
    const optiflow::core::Scenario scenario("unreachable_terminal",
                                            optiflow::core::State(0.0, 0.0),
                                            {optiflow::core::Exogenous(0.0, 0.0)},
                                            mp,
                                            terminal);

    bool threw = false;
    try {
        const std::vector<optiflow::core::DispatchStep> trajectory = solve_and_simulate(scenario, sp);
        static_cast<void>(trajectory);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
}

void test_csv_reader_accepts_complete_minimal_files() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::filesystem::path timeseries_path = directory / "optiflow_valid_timeseries.csv";
    const std::filesystem::path constraints_path = directory / "optiflow_valid_constraints.csv";

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

    const optiflow::core::ScenarioBundle bundle = optiflow::core::CsvScenarioReader::read(timeseries_path,
                                                                                          constraints_path);

    std::filesystem::remove(timeseries_path);
    std::filesystem::remove(constraints_path);

    assert(bundle.scenario.name() == "minimal_valid");
    assert(bundle.scenario.horizon_size() == 1);
    assert(near(bundle.scenario.initial_state().reservoir_volume, 10.0));
    assert(bundle.solver_parameters.reservoir_volume_grid_points == 2);
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
