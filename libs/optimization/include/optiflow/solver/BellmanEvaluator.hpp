#pragma once

#include <cstddef>

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/model/PumpedStorageModel.hpp>
#include <optiflow/numerics/ActionGrid.hpp>
#include <optiflow/numerics/StateGrid.hpp>
#include <optiflow/numerics/ValueFunction.hpp>

namespace optiflow {

/**
 * @brief Result of evaluating the Bellman action criterion at one state and time.
 */
struct BellmanDecision {
    HydroAction action{};
    TransitionResult transition{};
    double continuation_value_eur{};
    double total_value_eur{};
};

/**
 * @brief Shared one-step Bellman action evaluator.
 *
 * The backward solver and the forward rollout both use this helper so action
 * selection is based on the same criterion:
 * immediate reward plus discounted interpolated continuation value.
 */
class BellmanEvaluator {
public:
    /**
     * @brief Select the best action for one physical reservoir state.
     *
     * @throws std::runtime_error if no feasible action exists.
     */
    [[nodiscard]] static BellmanDecision select_action(const PumpedStorageModel& model,
                                                       const StateGrid& state_grid,
                                                       const ValueFunction& value_function,
                                                       const ActionGrid& action_grid,
                                                       std::size_t time_index,
                                                       double reservoir_volume_m3,
                                                       const ExogenousPoint& exogenous,
                                                       double discount_factor);
};

} // namespace optiflow
