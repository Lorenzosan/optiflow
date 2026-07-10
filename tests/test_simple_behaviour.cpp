#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cmath>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
namespace core = optiflow::core;
namespace model = optiflow::model;
namespace numerics = optiflow::numerics;
namespace solver = optiflow::solver;
constexpr double tolerance = 1.0e-9;
void near(double actual, double expected, std::string_view message) { if (std::abs(actual - expected) > tolerance) throw std::runtime_error(std::string(message)); }

core::ModelParameters parameters(double turbine_max, double pump_max, double operating_cost = 0.0) {
    return core::ModelParameters(1.0, 0.0, 10.0, turbine_max, pump_max, 0.0,
                                 1.0, 1.0, 1.0, operating_cost);
}

std::vector<core::DispatchStep> solve(const core::Scenario& scenario,
                                      const core::SolverParameters& solver_parameters) {
    const auto grid = numerics::StateGrid::from_parameters(scenario.model_parameters(), solver_parameters);
    const auto actions = numerics::ActionGrid::from_parameters(scenario.model_parameters(), solver_parameters);
    const model::PumpedStorageModel pumped_storage(scenario.model_parameters());
    const solver::BellmanResult result = solver::BellmanSolver(grid, actions, pumped_storage, solver_parameters).solve(scenario);
    return solver::ForwardSimulator(grid, actions, pumped_storage, solver_parameters)
        .simulate_from_value_function(scenario, result.value_function);
}

void test_zero_price_waits() {
    const core::Scenario scenario("wait", core::State(10.0), {core::Exogenous(0.0, 0.0)},
                                  parameters(10.0, 0.0),
                                  core::TerminalParameters(0.0, 10.0, 0.0, 0.0));
    const auto trajectory = solve(scenario, core::SolverParameters(2, 2, 1, 1, 1.0));
    near(trajectory.front().action.turbine_flow, 0.0, "wait action");
}

void test_high_price_turbines() {
    const core::Scenario scenario("generate", core::State(10.0), {core::Exogenous(100.0, 0.0)},
                                  parameters(10.0, 0.0),
                                  core::TerminalParameters(0.0, 10.0, 0.0, 0.0));
    const auto trajectory = solve(scenario, core::SolverParameters(2, 2, 1, 1, 1.0));
    near(trajectory.front().action.turbine_flow, 10.0, "turbine action");
    near(trajectory.front().net_power, 10.0, "generation");
}

void test_terminal_inventory_forces_wait() {
    const core::Scenario scenario("terminal", core::State(10.0), {core::Exogenous(100.0, 0.0)},
                                  parameters(10.0, 0.0),
                                  core::TerminalParameters(10.0, 10.0, 10.0, 0.0));
    const auto trajectory = solve(scenario, core::SolverParameters(2, 2, 1, 1, 1.0));
    near(trajectory.front().action.turbine_flow, 0.0, "terminal wait");
}

void test_terminal_inventory_forces_pumping() {
    const core::Scenario scenario("pump", core::State(0.0), {core::Exogenous(0.0, 0.0)},
                                  parameters(0.0, 10.0),
                                  core::TerminalParameters(10.0, 10.0, 10.0, 0.0));
    const auto trajectory = solve(scenario, core::SolverParameters(2, 1, 1, 2, 1.0));
    near(trajectory.front().action.pump_flow, 10.0, "pump action");
    near(trajectory.front().net_power, -10.0, "pump consumption");
}

}  // namespace

int main() {
    test_zero_price_waits();
    test_high_price_turbines();
    test_terminal_inventory_forces_wait();
    test_terminal_inventory_forces_pumping();
    return 0;
}
