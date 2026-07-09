#include "optiflow/runner/OptimizationRunner.h"

#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

namespace optiflow::runner {

OptimizationResult OptimizationRunner::run(const core::ScenarioBundle& bundle) const {
    const core::Scenario& scenario = bundle.scenario;
    const core::ModelParameters& model_parameters = scenario.model_parameters();
    const core::SolverParameters& solver_parameters = bundle.solver_parameters;

    const numerics::StateGrid state_grid =
        numerics::StateGrid::from_parameters(model_parameters, solver_parameters);

    const numerics::ActionGrid action_grid =
        numerics::ActionGrid::from_parameters(model_parameters, solver_parameters);

    const model::PumpedStorageModel model(model_parameters);

    const solver::BellmanSolver bellman_solver(
        state_grid,
        action_grid,
        model,
        solver_parameters);

    const solver::BellmanResult solution = bellman_solver.solve(scenario);

    const solver::ForwardSimulator simulator(
        state_grid,
        action_grid,
        model,
        solver_parameters);

    std::vector<core::DispatchStep> dispatch =
        simulator.simulate_from_value_function(scenario, solution.value_function);

    const double cumulative_profit =
        dispatch.empty() ? 0.0 : dispatch.back().cumulative_profit;

    return OptimizationResult{std::move(dispatch), cumulative_profit};
}

}  // namespace optiflow::runner
