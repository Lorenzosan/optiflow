#include "optiflow/solver/ForwardSimulator.h"

#include "optiflow/numerics/Interpolator.h"

#include <limits>
#include <stdexcept>
#include <utility>

namespace optiflow::solver {

ForwardSimulator::ForwardSimulator(numerics::StateGrid state_grid,
                                   numerics::ActionGrid action_grid,
                                   model::PumpedStorageModel model,
                                   core::SolverParameters solver_parameters)
    : state_grid_(std::move(state_grid)),
      action_grid_(std::move(action_grid)),
      model_(std::move(model)),
      solver_parameters_(solver_parameters) {
    core::validate_solver_parameters(solver_parameters_);
}

std::vector<core::DispatchStep> ForwardSimulator::simulate_greedy(
    const core::Scenario& scenario,
    const numerics::ValueFunction& value_function) const {
    std::vector<core::DispatchStep> trajectory;
    trajectory.reserve(scenario.horizon_size());

    core::State state = scenario.initial_state();
    double cumulative_profit = 0.0;

    for (std::size_t time_index = 0; time_index < scenario.horizon_size(); ++time_index) {
        const core::Exogenous& exogenous = scenario.exogenous_series().at(time_index);

        double best_value = -std::numeric_limits<double>::infinity();
        const core::Action* best_action = nullptr;
        core::Outcome best_outcome(state, 0.0, 0.0, 0.0, 0.0, false, "no feasible action");

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
                best_outcome = outcome;
            }
        }

        if (best_action == nullptr) {
            throw std::runtime_error("no feasible action found during forward simulation");
        }

        cumulative_profit += best_outcome.reward;
        trajectory.emplace_back(time_index,
                                state,
                                *best_action,
                                exogenous,
                                best_outcome.next_state,
                                best_outcome.net_power,
                                best_outcome.reward,
                                cumulative_profit);
        state = best_outcome.next_state;
    }

    return trajectory;
}

std::vector<core::DispatchStep> ForwardSimulator::simulate_nearest_policy(
    const core::Scenario& scenario,
    const numerics::Policy& policy) const {
    std::vector<core::DispatchStep> trajectory;
    trajectory.reserve(scenario.horizon_size());

    core::State state = scenario.initial_state();
    double cumulative_profit = 0.0;

    for (std::size_t time_index = 0; time_index < scenario.horizon_size(); ++time_index) {
        const numerics::StateIndex state_index = state_grid_.nearest_index(state);
        const core::Action& action = policy.get(time_index, state_index);
        const core::Exogenous& exogenous = scenario.exogenous_series().at(time_index);
        const core::Outcome outcome = model_.apply(state, action, exogenous);
        if (!outcome.feasible) {
            throw std::runtime_error("nearest-policy simulation selected an infeasible action");
        }

        cumulative_profit += outcome.reward;
        trajectory.emplace_back(time_index,
                                state,
                                action,
                                exogenous,
                                outcome.next_state,
                                outcome.net_power,
                                outcome.reward,
                                cumulative_profit);
        state = outcome.next_state;
    }

    return trajectory;
}

}  // namespace optiflow::solver
