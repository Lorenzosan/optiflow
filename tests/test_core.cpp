#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

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
    assert(std::abs(outcome.next_state.reservoir_volume - 42.0) < 1.0e-9);
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

void test_small_solve() {
    const optiflow::core::ModelParameters mp = model_parameters();
    const optiflow::core::SolverParameters sp = solver_parameters();
    const optiflow::core::Scenario scenario("unit",
                                            optiflow::core::State(50.0, 10.0),
                                            {optiflow::core::Exogenous(30.0, 2.0),
                                             optiflow::core::Exogenous(100.0, 2.0)},
                                            mp);

    const optiflow::numerics::StateGrid state_grid = optiflow::numerics::StateGrid::from_parameters(mp, sp);
    const optiflow::numerics::ActionGrid action_grid = optiflow::numerics::ActionGrid::from_parameters(mp, sp);
    const optiflow::model::PumpedStorageModel model(mp);
    const optiflow::solver::BellmanSolver solver(state_grid, action_grid, model, sp);
    const optiflow::solver::BellmanResult result = solver.solve(scenario);

    const optiflow::solver::ForwardSimulator simulator(state_grid, action_grid, model, sp);
    const std::vector<optiflow::core::DispatchStep> trajectory = simulator.simulate_greedy(scenario,
                                                                                           result.value_function);
    assert(trajectory.size() == 2);
}

}  // namespace

int main() {
    test_transition_model();
    test_infeasible_simultaneous_turbine_and_pump();
    test_small_solve();
    return 0;
}
