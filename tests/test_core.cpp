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

optiflow::core::ModelParameters model_parameters() {
    return optiflow::core::ModelParameters(1.0,
                                           0.0,
                                           100.0,
                                           0.0,
                                           20.0,
                                           20.0,
                                           10.0,
                                           20.0,
                                           5.0,
                                           5.0,
                                           0.9,
                                           0.85,
                                           0.95,
                                           0.95,
                                           0.5,
                                           2.0,
                                           1.0,
                                           1000000.0);
}

optiflow::core::TerminalParameters open_terminal_parameters() {
    return optiflow::core::TerminalParameters(0.0,
                                              100.0,
                                              0.0,
                                              20.0,
                                              50.0,
                                              10.0,
                                              0.0,
                                              0.0);
}

optiflow::core::SolverParameters solver_parameters() {
    return optiflow::core::SolverParameters(11, 5, 3, 1, 2, 2, 2, 1.0);
}

void test_transition_model() {
    const optiflow::model::PumpedStorageModel model(model_parameters());
    const optiflow::core::State state(50.0, 10.0);
    const optiflow::core::Action action(10.0, 0.0, 0.0, 0.0, 0.0);
    const optiflow::core::Exogenous exogenous(100.0, 2.0);

    const optiflow::core::Outcome outcome = model.apply(state, action, exogenous);

    assert(outcome.feasible);
    assert(near(outcome.next_state.reservoir_volume, 42.0));
    assert(outcome.net_power > 0.0);
    assert(outcome.reward > 0.0);
}

void test_infeasible_simultaneous_turbine_and_pump() {
    const optiflow::model::PumpedStorageModel model(model_parameters());
    const optiflow::core::Outcome outcome = model.apply(optiflow::core::State(50.0, 10.0),
                                                        optiflow::core::Action(10.0, 0.0, 5.0, 0.0, 0.0),
                                                        optiflow::core::Exogenous(100.0, 2.0));
    assert(!outcome.feasible);
}

void test_infeasible_simultaneous_battery_charge_and_discharge() {
    const optiflow::model::PumpedStorageModel model(model_parameters());
    const optiflow::core::Outcome outcome = model.apply(optiflow::core::State(50.0, 10.0),
                                                        optiflow::core::Action(0.0, 0.0, 0.0, 5.0, 5.0),
                                                        optiflow::core::Exogenous(100.0, 2.0));
    assert(!outcome.feasible);
}

void test_infeasible_next_state_outside_bounds() {
    const optiflow::model::PumpedStorageModel model(model_parameters());
    const optiflow::core::Outcome outcome = model.apply(optiflow::core::State(5.0, 10.0),
                                                        optiflow::core::Action(20.0, 0.0, 0.0, 0.0, 0.0),
                                                        optiflow::core::Exogenous(100.0, 0.0));
    assert(!outcome.feasible);
}

void test_scenario_rejects_initial_state_outside_bounds() {
    bool threw = false;
    try {
        const optiflow::core::Scenario scenario("bad_initial_state",
                                                optiflow::core::State(200.0, 10.0),
                                                {optiflow::core::Exogenous(30.0, 0.0)},
                                                model_parameters(),
                                                open_terminal_parameters());
        static_cast<void>(scenario);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void test_small_solve() {
    const optiflow::core::ModelParameters mp = model_parameters();
    const optiflow::core::SolverParameters sp = solver_parameters();
    const optiflow::core::Scenario scenario("unit",
                                            optiflow::core::State(50.0, 10.0),
                                            {optiflow::core::Exogenous(30.0, 2.0),
                                             optiflow::core::Exogenous(100.0, 2.0)},
                                            mp,
                                            open_terminal_parameters());

    const optiflow::numerics::StateGrid state_grid = optiflow::numerics::StateGrid::from_parameters(mp, sp);
    const optiflow::numerics::ActionGrid action_grid = optiflow::numerics::ActionGrid::from_parameters(mp, sp);
    const optiflow::model::PumpedStorageModel model(mp);
    const optiflow::solver::BellmanSolver solver(state_grid, action_grid, model, sp);
    const optiflow::solver::BellmanResult result = solver.solve(scenario);

    const optiflow::solver::ForwardSimulator simulator(state_grid, action_grid, model, sp);
    const std::vector<optiflow::core::DispatchStep> trajectory = simulator.simulate_from_value_function(scenario,
                                                                                           result.value_function);
    assert(trajectory.size() == 2);
}

void test_terminal_target_changes_final_state() {
    const optiflow::core::ModelParameters mp(1.0,
                                             0.0,
                                             100.0,
                                             0.0,
                                             0.0,
                                             20.0,
                                             0.0,
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
    const optiflow::core::SolverParameters sp(11, 1, 3, 1, 1, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(40.0,
                                                      40.0,
                                                      0.0,
                                                      0.0,
                                                      40.0,
                                                      0.0,
                                                      1000.0,
                                                      0.0);
    const optiflow::core::Scenario scenario("terminal_target",
                                            optiflow::core::State(50.0, 0.0),
                                            {optiflow::core::Exogenous(0.0, 0.0)},
                                            mp,
                                            terminal);

    const optiflow::numerics::StateGrid state_grid = optiflow::numerics::StateGrid::from_parameters(mp, sp);
    const optiflow::numerics::ActionGrid action_grid = optiflow::numerics::ActionGrid::from_parameters(mp, sp);
    const optiflow::model::PumpedStorageModel model(mp);
    const optiflow::solver::BellmanSolver solver(state_grid, action_grid, model, sp);
    const optiflow::solver::BellmanResult result = solver.solve(scenario);
    const optiflow::solver::ForwardSimulator simulator(state_grid, action_grid, model, sp);
    const std::vector<optiflow::core::DispatchStep> trajectory = simulator.simulate_from_value_function(scenario,
                                                                                           result.value_function);

    assert(trajectory.size() == 1);
    assert(near(trajectory.front().next_state.reservoir_volume, 40.0));
    assert(near(trajectory.front().next_state.battery_soc, 0.0));
}

void test_grid_forward_simulators_match_on_grid_aligned_case() {
    const optiflow::core::ModelParameters mp(1.0,
                                             0.0,
                                             100.0,
                                             0.0,
                                             0.0,
                                             10.0,
                                             0.0,
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
    const optiflow::core::SolverParameters sp(11, 1, 2, 1, 1, 1, 1, 1.0);
    const optiflow::core::TerminalParameters terminal(0.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    const optiflow::core::Scenario scenario("grid_aligned",
                                            optiflow::core::State(20.0, 0.0),
                                            {optiflow::core::Exogenous(100.0, 0.0),
                                             optiflow::core::Exogenous(90.0, 0.0)},
                                            mp,
                                            terminal);

    const optiflow::numerics::StateGrid state_grid = optiflow::numerics::StateGrid::from_parameters(mp, sp);
    const optiflow::numerics::ActionGrid action_grid = optiflow::numerics::ActionGrid::from_parameters(mp, sp);
    const optiflow::model::PumpedStorageModel model(mp);
    const optiflow::solver::BellmanSolver solver(state_grid, action_grid, model, sp);
    const optiflow::solver::BellmanResult result = solver.solve(scenario);
    const optiflow::solver::ForwardSimulator simulator(state_grid, action_grid, model, sp);

    const std::vector<optiflow::core::DispatchStep> value_based = simulator.simulate_from_value_function(scenario,
                                                                                       result.value_function);
    const std::vector<optiflow::core::DispatchStep> nearest = simulator.simulate_nearest_policy(scenario,
                                                                                                result.policy);

    assert(value_based.size() == nearest.size());
    for (std::size_t i = 0; i < value_based.size(); ++i) {
        assert(near(value_based.at(i).state.reservoir_volume, nearest.at(i).state.reservoir_volume));
        assert(near(value_based.at(i).next_state.reservoir_volume, nearest.at(i).next_state.reservoir_volume));
        assert(near(value_based.at(i).action.turbine_flow, nearest.at(i).action.turbine_flow));
        assert(near(value_based.at(i).reward, nearest.at(i).reward));
    }
}

void test_csv_reader_rejects_missing_terminal_key() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::filesystem::path timeseries_path = directory / "optiflow_test_timeseries.csv";
    const std::filesystem::path constraints_path = directory / "optiflow_test_constraints.csv";

    {
        std::ofstream timeseries(timeseries_path);
        timeseries << "time_index,price,natural_inflow\n";
        timeseries << "0,0,0\n";
    }
    {
        std::ofstream constraints(constraints_path);
        constraints << "key,value\n";
        constraints << "scenario_name,missing_terminal\n";
        constraints << "time_step_hours,1\n";
        constraints << "reservoir_min_volume,0\n";
        constraints << "reservoir_max_volume,100\n";
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
        constraints << "initial_reservoir_volume,50\n";
        constraints << "initial_battery_soc,0\n";
        constraints << "reservoir_volume_grid_points,11\n";
        constraints << "battery_soc_grid_points,1\n";
        constraints << "turbine_flow_steps,2\n";
        constraints << "spill_flow_steps,1\n";
        constraints << "pump_flow_steps,1\n";
        constraints << "battery_charge_steps,1\n";
        constraints << "battery_discharge_steps,1\n";
        constraints << "discount_factor,1\n";
    }

    bool threw = false;
    try {
        const optiflow::core::ScenarioBundle bundle = optiflow::core::CsvScenarioReader::read(timeseries_path,
                                                                                              constraints_path);
        static_cast<void>(bundle);
    } catch (const std::invalid_argument& error) {
        threw = std::string(error.what()).find("terminal_reservoir_min_volume") != std::string::npos;
    }

    std::filesystem::remove(timeseries_path);
    std::filesystem::remove(constraints_path);
    assert(threw);
}

}  // namespace

int main() {
    test_transition_model();
    test_infeasible_simultaneous_turbine_and_pump();
    test_infeasible_simultaneous_battery_charge_and_discharge();
    test_infeasible_next_state_outside_bounds();
    test_scenario_rejects_initial_state_outside_bounds();
    test_small_solve();
    test_terminal_target_changes_final_state();
    test_grid_forward_simulators_match_on_grid_aligned_case();
    test_csv_reader_rejects_missing_terminal_key();
    return 0;
}
