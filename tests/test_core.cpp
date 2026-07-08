#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Interpolator.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"
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

core::ModelParameters model_parameters() {
    return core::ModelParameters(1.0,
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

core::TerminalParameters open_terminal_parameters() {
    return core::TerminalParameters(0.0,
                                    100.0,
                                    0.0,
                                    20.0,
                                    50.0,
                                    10.0,
                                    0.0,
                                    0.0);
}

core::SolverParameters solver_parameters() {
    return core::SolverParameters(11, 5, 3, 1, 2, 2, 2, 1.0);
}

void test_transition_model_updates_state_power_and_reward() {
    const model::PumpedStorageModel pumped_storage_model(model_parameters());
    const core::State state(50.0, 10.0);
    const core::Action action(10.0, 0.0, 0.0, 0.0, 0.0);
    const core::Exogenous exogenous(100.0, 2.0);

    const core::Outcome outcome = pumped_storage_model.apply(state, action, exogenous);

    require(outcome.feasible, "transition should be feasible");
    require_near(outcome.next_state.reservoir_volume, 42.0, "next reservoir volume");
    require_near(outcome.next_state.battery_soc, 10.0, "next battery state of charge");
    require_near(outcome.turbine_power, 4.5, "turbine power");
    require_near(outcome.pump_power, 0.0, "pump power");
    require_near(outcome.net_power, 4.5, "net power");
    require_near(outcome.reward, 445.5, "reward");
    require(outcome.infeasibility_reason.empty(), "feasible transition should not have an infeasibility reason");
}

void test_transition_model_rejects_mutually_exclusive_actions() {
    const model::PumpedStorageModel pumped_storage_model(model_parameters());
    const core::State state(50.0, 10.0);
    const core::Exogenous exogenous(100.0, 2.0);

    const core::Outcome turbine_and_pump = pumped_storage_model.apply(
        state,
        core::Action(10.0, 0.0, 5.0, 0.0, 0.0),
        exogenous);
    require(!turbine_and_pump.feasible, "simultaneous turbine and pump should be infeasible");
    require(turbine_and_pump.infeasibility_reason.find("turbine and pump") != std::string::npos,
            "infeasibility reason should mention turbine and pump");

    const core::Outcome charge_and_discharge = pumped_storage_model.apply(
        state,
        core::Action(0.0, 0.0, 0.0, 5.0, 5.0),
        exogenous);
    require(!charge_and_discharge.feasible, "simultaneous charge and discharge should be infeasible");
    require(charge_and_discharge.infeasibility_reason.find("charge and discharge") != std::string::npos,
            "infeasibility reason should mention charge and discharge");
}

void test_transition_model_rejects_next_state_outside_bounds() {
    const model::PumpedStorageModel pumped_storage_model(model_parameters());

    const core::Outcome outcome = pumped_storage_model.apply(core::State(5.0, 10.0),
                                                             core::Action(20.0, 0.0, 0.0, 0.0, 0.0),
                                                             core::Exogenous(100.0, 0.0));

    require(!outcome.feasible, "transition outside reservoir bounds should be infeasible");
    require(outcome.reward < 0.0, "infeasible transition should carry a negative penalty");
    require(outcome.infeasibility_reason.find("reservoir") != std::string::npos,
            "infeasibility reason should mention reservoir");
}

void test_scenario_rejects_initial_state_outside_bounds() {
    require_throws<std::invalid_argument>([]() {
        const core::Scenario scenario("bad_initial_state",
                                      core::State(200.0, 10.0),
                                      {core::Exogenous(30.0, 0.0)},
                                      model_parameters(),
                                      open_terminal_parameters());
        static_cast<void>(scenario);
    }, "initial_reservoir_volume");
}

void test_state_grid_coordinates_and_nearest_index() {
    const numerics::StateGrid grid(0.0, 100.0, 6, 0.0, 20.0, 3);

    require(grid.reservoir_size() == 6, "reservoir grid size");
    require(grid.battery_size() == 3, "battery grid size");
    require_near(grid.reservoir_volume_at(2), 40.0, "reservoir coordinate");
    require_near(grid.battery_soc_at(1), 10.0, "battery coordinate");

    const core::State state = grid.state_at(numerics::StateIndex(3, 2));
    require_near(state.reservoir_volume, 60.0, "state reservoir coordinate");
    require_near(state.battery_soc, 20.0, "state battery coordinate");

    const numerics::StateIndex nearest = grid.nearest_index(core::State(61.0, 12.0));
    require(nearest.reservoir_index == 3, "nearest reservoir index");
    require(nearest.battery_index == 1, "nearest battery index");

    require(grid.contains(core::State(100.0, 20.0)), "upper grid bound should be contained");
    require(!grid.contains(core::State(101.0, 20.0)), "state outside grid should not be contained");
}

void test_value_function_policy_and_interpolation() {
    const numerics::StateGrid grid(0.0, 10.0, 2, 0.0, 10.0, 2);
    numerics::ValueFunction value_function(1, grid);

    value_function.set(1, numerics::StateIndex(0, 0), 0.0);
    value_function.set(1, numerics::StateIndex(1, 0), 10.0);
    value_function.set(1, numerics::StateIndex(0, 1), 20.0);
    value_function.set(1, numerics::StateIndex(1, 1), 30.0);

    require_near(value_function.get(1, numerics::StateIndex(1, 1)), 30.0, "value-function storage");
    require_near(numerics::Interpolator::bilinear(value_function, grid, 1, core::State(0.0, 0.0)),
                 0.0,
                 "interpolation at lower grid point");
    require_near(numerics::Interpolator::bilinear(value_function, grid, 1, core::State(5.0, 5.0)),
                 15.0,
                 "interpolation at cell midpoint");
    require_throws<std::out_of_range>([&]() {
        static_cast<void>(numerics::Interpolator::bilinear(value_function, grid, 1, core::State(11.0, 5.0)));
    }, "reservoir");

    numerics::Policy policy(1, grid);
    const numerics::StateIndex policy_index(1, 0);
    require(!policy.has_action(0, policy_index), "policy entry should initially be empty");
    policy.set(0, policy_index, core::Action(3.0, 0.0, 0.0, 0.0, 0.0));
    require(policy.has_action(0, policy_index), "policy entry should be set");
    require_near(policy.get(0, policy_index).turbine_flow, 3.0, "stored policy action");
}

void test_small_bellman_solve_and_forward_simulation() {
    const core::ModelParameters mp = model_parameters();
    const core::SolverParameters sp = solver_parameters();
    const core::Scenario scenario("unit",
                                  core::State(50.0, 10.0),
                                  {core::Exogenous(30.0, 2.0), core::Exogenous(100.0, 2.0)},
                                  mp,
                                  open_terminal_parameters());

    const numerics::StateGrid state_grid = numerics::StateGrid::from_parameters(mp, sp);
    const numerics::ActionGrid action_grid = numerics::ActionGrid::from_parameters(mp, sp);
    const model::PumpedStorageModel pumped_storage_model(mp);
    const solver::BellmanSolver bellman_solver(state_grid, action_grid, pumped_storage_model, sp);
    const solver::BellmanResult result = bellman_solver.solve(scenario);

    const solver::ForwardSimulator simulator(state_grid, action_grid, pumped_storage_model, sp);
    const std::vector<core::DispatchStep> trajectory = simulator.simulate_from_value_function(
        scenario,
        result.value_function);

    require(trajectory.size() == 2, "forward trajectory length");
    require(result.policy.horizon_size() == 2, "policy horizon length");
}

void test_terminal_target_changes_final_state() {
    const core::ModelParameters mp(1.0,
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
    const core::SolverParameters sp(11, 1, 3, 1, 1, 1, 1, 1.0);
    const core::TerminalParameters terminal(40.0, 40.0, 0.0, 0.0, 40.0, 0.0, 1000.0, 0.0);
    const core::Scenario scenario("terminal_target",
                                  core::State(50.0, 0.0),
                                  {core::Exogenous(0.0, 0.0)},
                                  mp,
                                  terminal);

    const numerics::StateGrid state_grid = numerics::StateGrid::from_parameters(mp, sp);
    const numerics::ActionGrid action_grid = numerics::ActionGrid::from_parameters(mp, sp);
    const model::PumpedStorageModel pumped_storage_model(mp);
    const solver::BellmanSolver bellman_solver(state_grid, action_grid, pumped_storage_model, sp);
    const solver::BellmanResult result = bellman_solver.solve(scenario);
    const solver::ForwardSimulator simulator(state_grid, action_grid, pumped_storage_model, sp);
    const std::vector<core::DispatchStep> trajectory = simulator.simulate_from_value_function(
        scenario,
        result.value_function);

    require(trajectory.size() == 1, "terminal-target trajectory length");
    require_near(trajectory.front().next_state.reservoir_volume, 40.0, "terminal reservoir volume");
    require_near(trajectory.front().next_state.battery_soc, 0.0, "terminal battery state of charge");
}

void test_nearest_policy_matches_value_function_simulation_on_grid_aligned_case() {
    const core::ModelParameters mp(1.0,
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
    const core::SolverParameters sp(11, 1, 2, 1, 1, 1, 1, 1.0);
    const core::TerminalParameters terminal(0.0, 100.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
    const core::Scenario scenario("grid_aligned",
                                  core::State(20.0, 0.0),
                                  {core::Exogenous(100.0, 0.0), core::Exogenous(90.0, 0.0)},
                                  mp,
                                  terminal);

    const numerics::StateGrid state_grid = numerics::StateGrid::from_parameters(mp, sp);
    const numerics::ActionGrid action_grid = numerics::ActionGrid::from_parameters(mp, sp);
    const model::PumpedStorageModel pumped_storage_model(mp);
    const solver::BellmanSolver bellman_solver(state_grid, action_grid, pumped_storage_model, sp);
    const solver::BellmanResult result = bellman_solver.solve(scenario);
    const solver::ForwardSimulator simulator(state_grid, action_grid, pumped_storage_model, sp);

    const std::vector<core::DispatchStep> value_function_trajectory = simulator.simulate_from_value_function(
        scenario,
        result.value_function);
    const std::vector<core::DispatchStep> policy_trajectory = simulator.simulate_nearest_policy(scenario,
                                                                                                 result.policy);

    require(value_function_trajectory.size() == policy_trajectory.size(), "trajectory sizes should match");
    for (std::size_t index = 0; index < value_function_trajectory.size(); ++index) {
        require_near(value_function_trajectory.at(index).state.reservoir_volume,
                     policy_trajectory.at(index).state.reservoir_volume,
                     "simulator state reservoir volume");
        require_near(value_function_trajectory.at(index).next_state.reservoir_volume,
                     policy_trajectory.at(index).next_state.reservoir_volume,
                     "simulator next reservoir volume");
        require_near(value_function_trajectory.at(index).action.turbine_flow,
                     policy_trajectory.at(index).action.turbine_flow,
                     "simulator turbine flow");
        require_near(value_function_trajectory.at(index).reward,
                     policy_trajectory.at(index).reward,
                     "simulator reward");
    }
}

void test_csv_reader_rejects_missing_terminal_key() {
    const std::filesystem::path directory = std::filesystem::temp_directory_path();
    const std::filesystem::path scenario_path = directory / "optiflow_test_core_scenario.csv";
    const std::filesystem::path prices_path = directory / "optiflow_test_core_prices.csv";
    const std::filesystem::path inflows_path = directory / "optiflow_test_core_inflows.csv";

    {
        std::ofstream prices(prices_path);
        prices << "time_index,price\n";
        prices << "0,0\n";
    }
    {
        std::ofstream inflows(inflows_path);
        inflows << "time_index,natural_inflow\n";
        inflows << "0,0\n";
    }
    {
        std::ofstream scenario(scenario_path);
        scenario << "key,value\n";
        scenario << "scenario_name,missing_terminal\n";
        scenario << "time_step_hours,1\n";
        scenario << "reservoir_min_volume,0\n";
        scenario << "reservoir_max_volume,100\n";
        scenario << "battery_min_soc,0\n";
        scenario << "battery_max_soc,0\n";
        scenario << "turbine_max_flow,10\n";
        scenario << "pump_max_flow,0\n";
        scenario << "spill_max_flow,0\n";
        scenario << "battery_max_charge_power,0\n";
        scenario << "battery_max_discharge_power,0\n";
        scenario << "turbine_efficiency,1\n";
        scenario << "pump_efficiency,1\n";
        scenario << "battery_charge_efficiency,1\n";
        scenario << "battery_discharge_efficiency,1\n";
        scenario << "water_to_power_factor,1\n";
        scenario << "battery_degradation_cost_per_mwh,0\n";
        scenario << "operating_cost_per_mwh,0\n";
        scenario << "infeasibility_penalty,1000000\n";
        scenario << "terminal_reservoir_max_volume,100\n";
        scenario << "terminal_battery_min_soc,0\n";
        scenario << "terminal_battery_max_soc,0\n";
        scenario << "terminal_target_reservoir_volume,0\n";
        scenario << "terminal_target_battery_soc,0\n";
        scenario << "terminal_reservoir_target_penalty,0\n";
        scenario << "terminal_battery_target_penalty,0\n";
        scenario << "initial_reservoir_volume,50\n";
        scenario << "initial_battery_soc,0\n";
        scenario << "reservoir_volume_grid_points,11\n";
        scenario << "battery_soc_grid_points,1\n";
        scenario << "turbine_flow_steps,2\n";
        scenario << "spill_flow_steps,1\n";
        scenario << "pump_flow_steps,1\n";
        scenario << "battery_charge_steps,1\n";
        scenario << "battery_discharge_steps,1\n";
        scenario << "discount_factor,1\n";
    }

    require_throws<std::invalid_argument>([&]() {
        const core::ScenarioBundle bundle = core::CsvScenarioReader::read(scenario_path,
                                                                          prices_path,
                                                                          inflows_path);
        static_cast<void>(bundle);
    }, "terminal_reservoir_min_volume");

    std::filesystem::remove(scenario_path);
    std::filesystem::remove(prices_path);
    std::filesystem::remove(inflows_path);
}

}  // namespace

int main() {
    test_transition_model_updates_state_power_and_reward();
    test_transition_model_rejects_mutually_exclusive_actions();
    test_transition_model_rejects_next_state_outside_bounds();
    test_scenario_rejects_initial_state_outside_bounds();
    test_state_grid_coordinates_and_nearest_index();
    test_value_function_policy_and_interpolation();
    test_small_bellman_solve_and_forward_simulation();
    test_terminal_target_changes_final_state();
    test_nearest_policy_matches_value_function_simulation_on_grid_aligned_case();
    test_csv_reader_rejects_missing_terminal_key();
    return 0;
}
