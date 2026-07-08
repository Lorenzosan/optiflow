#include <optiflow/solver/BellmanSolver.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include <optiflow/model/PumpedStorageModel.hpp>
#include <optiflow/model/TerminalPenaltyModel.hpp>
#include <optiflow/numerics/ActionGrid.hpp>
#include <optiflow/numerics/Interpolator.hpp>
#include <optiflow/solver/BellmanEvaluator.hpp>

namespace optiflow {
namespace {

constexpr double anchor_tolerance = 1.0e-9;

void sort_and_deduplicate(std::vector<double>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(), [](double lhs, double rhs) {
                     return std::abs(lhs - rhs) <= anchor_tolerance;
                 }),
                 values.end());
}

std::vector<double> make_reachable_state_anchors(const PumpedStorageModel& model,
                                                 const ActionGrid& action_grid,
                                                 const DeterministicSeries& series,
                                                 double initial_reservoir_volume_m3) {
    std::vector<double> anchors{initial_reservoir_volume_m3};
    std::vector<double> current_states{initial_reservoir_volume_m3};

    for (const auto& exogenous : series.points) {
        std::vector<double> next_states;
        next_states.reserve(current_states.size() * action_grid.size());

        for (const double reservoir_volume_m3 : current_states) {
            const ReservoirState state{reservoir_volume_m3};
            for (const auto& action : action_grid.actions()) {
                const auto transition = model.transition(state, action, exogenous);
                if (!transition.feasible) {
                    continue;
                }
                next_states.push_back(transition.next_reservoir_volume_m3);
            }
        }

        sort_and_deduplicate(next_states);
        anchors.insert(anchors.end(), next_states.begin(), next_states.end());
        current_states = std::move(next_states);
    }

    sort_and_deduplicate(anchors);
    return anchors;
}

} // namespace

OptimizationResult BellmanSolver::solve(const DeterministicProblem& problem) const {
    validate_problem(problem);

    const auto parameters = to_model_parameters(problem);
    const auto& series = problem.exogenous;
    const auto& config = problem.solver;

    PumpedStorageModel model(parameters);
    const ActionGrid action_grid(ActionGridConfig{parameters.max_turbine_flow_m3_s,
                                                  parameters.max_pump_flow_m3_s,
                                                  config.turbine_flow_steps,
                                                  config.pump_flow_steps});

    auto state_anchors = make_reachable_state_anchors(model,
                                                       action_grid,
                                                       series,
                                                       parameters.initial_reservoir_volume_m3);
    state_anchors.push_back(parameters.initial_reservoir_volume_m3);
    state_anchors.push_back(parameters.target_final_reservoir_volume_m3);

    const StateGrid state_grid(parameters.min_reservoir_volume_m3,
                               parameters.max_reservoir_volume_m3,
                               config.volume_grid_points,
                               state_anchors);

    ValueFunction value_function(series.size(), state_grid.size());
    Policy policy(series.size(), state_grid.size());
    const TerminalPenaltyModel terminal_penalty(parameters.target_final_reservoir_volume_m3,
                                                parameters.terminal_reservoir_penalty_eur_per_m3);

    const std::size_t terminal_time = series.size();
    for (std::size_t state_index = 0; state_index < state_grid.size(); ++state_index) {
        value_function.set(terminal_time, state_index, terminal_penalty.value(state_grid.at(state_index)));
    }

    for (std::size_t reverse_time = series.size(); reverse_time > 0; --reverse_time) {
        const std::size_t time_index = reverse_time - 1;
        const auto& exogenous = series.points.at(time_index);

        for (std::size_t state_index = 0; state_index < state_grid.size(); ++state_index) {
            const auto decision = BellmanEvaluator::select_action(model,
                                                                  state_grid,
                                                                  value_function,
                                                                  action_grid,
                                                                  time_index,
                                                                  state_grid.at(state_index),
                                                                  exogenous,
                                                                  parameters.discount_factor);
            value_function.set(time_index, state_index, decision.total_value_eur);
            policy.set(time_index, state_index, decision.action);
        }
    }

    const double objective = Interpolator::interpolate(state_grid,
                                                       value_function,
                                                       0,
                                                       parameters.initial_reservoir_volume_m3);

    return OptimizationResult{state_grid, value_function, policy, problem, objective};
}

} // namespace optiflow
