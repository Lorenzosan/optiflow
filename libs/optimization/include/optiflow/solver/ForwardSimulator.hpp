#pragma once

#include <vector>

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/solver/OptimizationResult.hpp>

namespace optiflow {

/**
 * @brief One simulated dispatch step from a solved deterministic problem.
 */
struct DispatchStep {
    std::size_t time_index{};
    double price_eur_per_mwh{};
    double natural_inflow_m3_s{};
    double reservoir_start_m3{};
    double reservoir_end_m3{};
    HydroAction action{};
    double turbine_power_mw{};
    double pump_power_mw{};
    double net_power_mw{};
    double overflow_spill_m3{};
    double reward_eur{};
};

/**
 * @brief Forward rollout using the solved Bellman value functions.
 *
 * The simulator re-evaluates the Bellman action criterion at the actual
 * reservoir volume reached during the rollout. This avoids replaying a
 * nearest-grid policy when the physical state is between grid points.
 */
class ForwardSimulator {
public:
    /**
     * @brief Simulate the solved problem from the initial reservoir volume.
     */
    [[nodiscard]] static std::vector<DispatchStep> simulate(const OptimizationResult& result);
};

} // namespace optiflow
