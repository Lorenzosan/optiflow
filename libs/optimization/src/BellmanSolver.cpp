#include "optiflow/solver/BellmanSolver.h"

#include "optiflow/numerics/Interpolator.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace optiflow::solver {

BellmanResult::BellmanResult(numerics::ValueFunction value_function_value,
                             numerics::Policy policy_value)
    : value_function(std::move(value_function_value)), policy(std::move(policy_value)) {}

BellmanSolver::BellmanSolver(numerics::StateGrid state_grid,
                             numerics::ActionGrid action_grid,
                             model::PumpedStorageModel model,
                             core::SolverParameters solver_parameters)
    : state_grid_(std::move(state_grid)),
      action_grid_(std::move(action_grid)),
      model_(std::move(model)),
      solver_parameters_(solver_parameters) {
    core::validate_solver_parameters(solver_parameters_);
}

BellmanResult BellmanSolver::solve(const core::Scenario& scenario) const {
    numerics::ValueFunction value_function(scenario.horizon_size(), state_grid_);
    numerics::Policy policy(scenario.horizon_size(), state_grid_);

    const auto& exogenous_series = scenario.exogenous_series();

    for (std::size_t reverse_t = scenario.horizon_size(); reverse_t > 0; --reverse_t) {
        const std::size_t time_index = reverse_t - 1;
        const core::Exogenous& exogenous = exogenous_series.at(time_index);

        for (std::size_t reservoir_index = 0; reservoir_index < state_grid_.reservoir_size(); ++reservoir_index) {
            for (std::size_t battery_index = 0; battery_index < state_grid_.battery_size(); ++battery_index) {
                const numerics::StateIndex state_index(reservoir_index, battery_index);
                const core::State state = state_grid_.state_at(state_index);

                double best_value = -std::numeric_limits<double>::infinity();
                const core::Action* best_action = nullptr;

                for (const core::Action& action : action_grid_.actions()) {
                    const core::Outcome outcome = model_.apply(state, action, exogenous);
                    if (!outcome.feasible) {
                        continue;
                    }
                    const double continuation = numerics::Interpolator::bilinear(value_function,
                                                                                  state_grid_,
                                                                                  time_index + 1,
                                                                                  outcome.next_state);
                    const double candidate = outcome.reward + solver_parameters_.discount_factor * continuation;
                    if (candidate > best_value) {
                        best_value = candidate;
                        best_action = &action;
                    }
                }

                if (best_action == nullptr) {
                    throw std::runtime_error("no feasible action found at a grid state");
                }

                value_function.set(time_index, state_index, best_value);
                policy.set(time_index, state_index, *best_action);
            }
        }
    }

    return BellmanResult(std::move(value_function), std::move(policy));
}

}  // namespace optiflow::solver
