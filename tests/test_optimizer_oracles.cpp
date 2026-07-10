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

core::ModelParameters parameters(double operating_cost = 0.0) {
    return core::ModelParameters(1.0, 0.0, 10.0, 10.0, 10.0, 0.0,
                                 1.0, 1.0, 1.0, operating_cost);
}
std::vector<core::DispatchStep> solve(const core::Scenario& scenario) {
    const core::SolverParameters sp(2, 2, 1, 2, 1.0);
    const auto grid = numerics::StateGrid::from_parameters(scenario.model_parameters(), sp);
    const auto actions = numerics::ActionGrid::from_parameters(scenario.model_parameters(), sp);
    const model::PumpedStorageModel pumped_storage(scenario.model_parameters());
    const auto result = solver::BellmanSolver(grid, actions, pumped_storage, sp).solve(scenario);
    return solver::ForwardSimulator(grid, actions, pumped_storage, sp)
        .simulate_from_value_function(scenario, result.value_function);
}
core::TerminalParameters open_terminal() { return core::TerminalParameters(0.0, 10.0, 0.0, 0.0); }

void test_high_price_now_generates() {
    const auto trajectory = solve(core::Scenario(
        "high_now", core::State(10.0),
        {core::Exogenous(100.0, 0.0), core::Exogenous(0.0, 0.0)},
        parameters(), open_terminal()));
    near(trajectory[0].action.turbine_flow, 10.0, "generate now");
    near(trajectory.back().cumulative_profit, 1000.0, "profit");
}

void test_low_price_now_preserves_water() {
    const auto trajectory = solve(core::Scenario(
        "high_later", core::State(10.0),
        {core::Exogenous(0.0, 0.0), core::Exogenous(100.0, 0.0)},
        parameters(), open_terminal()));
    near(trajectory[0].action.turbine_flow, 0.0, "preserve water");
    near(trajectory[1].action.turbine_flow, 10.0, "generate later");
}

void test_negative_price_pumps_for_later_generation() {
    const auto trajectory = solve(core::Scenario(
        "pump_then_generate", core::State(0.0),
        {core::Exogenous(-10.0, 0.0), core::Exogenous(100.0, 0.0)},
        parameters(), open_terminal()));
    near(trajectory[0].action.pump_flow, 10.0, "pump at negative price");
    near(trajectory[1].action.turbine_flow, 10.0, "generate later");
    near(trajectory.back().cumulative_profit, 1100.0, "arbitrage profit");
}

void test_high_operating_cost_avoids_cycle() {
    const auto trajectory = solve(core::Scenario(
        "expensive", core::State(0.0),
        {core::Exogenous(0.0, 0.0), core::Exogenous(100.0, 0.0)},
        parameters(75.0), open_terminal()));
    near(trajectory[0].action.pump_flow, 0.0, "avoid expensive pumping");
    near(trajectory.back().cumulative_profit, 0.0, "no-cycle profit");
}

}  // namespace

int main() {
    test_high_price_now_generates();
    test_low_price_now_preserves_water();
    test_negative_price_pumps_for_later_generation();
    test_high_operating_cost_avoids_cycle();
    return 0;
}
