#pragma once

#include "optiflow/core/StorageTypes.h"

namespace optiflow::solver {

/**
 * @brief Return true when a candidate should replace the current best action.
 *
 * A strictly larger value always wins. Exactly equal values use a deterministic
 * operational tie-break: less spill, then less total controlled hydraulic
 * throughput, then less pumping, then less turbine withdrawal.
 */
bool better_action_candidate(double candidate_value,
                             const core::Action& candidate_action,
                             double best_value,
                             const core::Action* best_action);

}  // namespace optiflow::solver
