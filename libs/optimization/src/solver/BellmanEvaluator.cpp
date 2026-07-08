#include <optiflow/solver/BellmanEvaluator.hpp>

#include <cmath>
#include <limits>
#include <stdexcept>

#include <optiflow/numerics/Interpolator.hpp>

namespace optiflow {

BellmanDecision BellmanEvaluator::select_action(const PumpedStorageModel& model,
                                                const StateGrid& state_grid,
                                                const ValueFunction& value_function,
                                                const ActionGrid& action_grid,
                                                std::size_t time_index,
                                                double reservoir_volume_m3,
                                                const ExogenousPoint& exogenous,
                                                double discount_factor) {
    double best_value = -std::numeric_limits<double>::infinity();
    BellmanDecision best_decision{};

    const ReservoirState state{reservoir_volume_m3};
    for (const auto& action : action_grid.actions()) {
        const auto transition = model.transition(state, action, exogenous);
        if (!transition.feasible) {
            continue;
        }

        const double continuation = Interpolator::interpolate(state_grid,
                                                              value_function,
                                                              time_index + 1,
                                                              transition.next_reservoir_volume_m3);
        const double candidate_value = transition.reward_eur + discount_factor * continuation;

        if (candidate_value > best_value) {
            best_value = candidate_value;
            best_decision = BellmanDecision{action, transition, continuation, candidate_value};
        }
    }

    if (!std::isfinite(best_value)) {
        throw std::runtime_error("no feasible action found for Bellman action selection");
    }

    return best_decision;
}

} // namespace optiflow
