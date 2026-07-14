#include "optiflow/solver/ActionSelection.h"

#include <tuple>

namespace optiflow::solver {

namespace {

auto action_rank(const core::Action& action) {
    return std::tuple(action.spill_flow,
                      action.turbine_flow + action.pump_flow,
                      action.pump_flow,
                      action.turbine_flow);
}

}  // namespace

bool better_action_candidate(double candidate_value,
                             const core::Action& candidate_action,
                             double best_value,
                             const core::Action* best_action) {
    if (best_action == nullptr || candidate_value > best_value) {
        return true;
    }
    if (candidate_value < best_value) {
        return false;
    }
    return action_rank(candidate_action) < action_rank(*best_action);
}

}  // namespace optiflow::solver
