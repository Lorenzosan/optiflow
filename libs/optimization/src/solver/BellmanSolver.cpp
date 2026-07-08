#include <optiflow/solver/BellmanSolver.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

#include <optiflow/model/PumpedStorageModel.hpp>
#include <optiflow/model/TerminalValueModel.hpp>
#include <optiflow/numerics/ActionGrid.hpp>
#include <optiflow/numerics/Interpolator.hpp>

namespace optiflow {

BellmanSolver::BellmanSolver(BellmanSolverConfig config) : config_(config) {
    if (config_.volume_grid_points < 2) {
        throw std::invalid_argument("volume grid must contain at least two points");
    }
}

OptimizationResult BellmanSolver::solve(const DeterministicSeries& series,
                                         const ModelParameters& parameters) const {
    if (series.empty()) {
        throw std::invalid_argument("deterministic series must not be empty");
    }

    PumpedStorageModel model(parameters);
    const StateGrid state_grid(parameters.min_reservoir_volume_m3,
                               parameters.max_reservoir_volume_m3,
                               config_.volume_grid_points);
    const ActionGrid action_grid(ActionGridConfig{parameters.max_turbine_flow_m3_s,
                                                  parameters.max_pump_flow_m3_s,
                                                  config_.turbine_flow_steps,
                                                  config_.pump_flow_steps});

    ValueFunction value_function(series.size(), state_grid.size());
    Policy policy(series.size(), state_grid.size());
    const TerminalValueModel terminal_value(parameters.terminal_water_value_eur_per_m3);

    const std::size_t terminal_time = series.size();
    for (std::size_t state_index = 0; state_index < state_grid.size(); ++state_index) {
        value_function.set(terminal_time, state_index, terminal_value.value(state_grid.at(state_index)));
    }

    for (std::size_t reverse_time = series.size(); reverse_time > 0; --reverse_time) {
        const std::size_t time_index = reverse_time - 1;
        const auto& exogenous = series.points.at(time_index);

        for (std::size_t state_index = 0; state_index < state_grid.size(); ++state_index) {
            const ReservoirState state{state_grid.at(state_index)};
            double best_value = -std::numeric_limits<double>::infinity();
            HydroAction best_action{};

            for (const auto& action : action_grid.actions()) {
                const auto transition = model.transition(state, action, exogenous);
                if (!transition.feasible) {
                    continue;
                }

                const double continuation = Interpolator::interpolate(state_grid,
                                                                       value_function,
                                                                       time_index + 1,
                                                                       transition.next_reservoir_volume_m3);
                const double candidate_value = transition.reward_eur +
                                               parameters.discount_factor * continuation;

                if (candidate_value > best_value) {
                    best_value = candidate_value;
                    best_action = action;
                }
            }

            if (!std::isfinite(best_value)) {
                throw std::runtime_error("no feasible action found during Bellman recursion");
            }

            value_function.set(time_index, state_index, best_value);
            policy.set(time_index, state_index, best_action);
        }
    }

    const double objective = Interpolator::interpolate(state_grid,
                                                       value_function,
                                                       0,
                                                       parameters.initial_reservoir_volume_m3);

    return OptimizationResult{state_grid, value_function, policy, parameters, objective};
}

} // namespace optiflow
