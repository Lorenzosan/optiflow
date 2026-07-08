#pragma once

#include <vector>

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/solver/OptimizationResult.hpp>

namespace optiflow {

/**
 * @brief One simulated dispatch step from a solved deterministic policy.
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
 * @brief Forward simulation of a solved deterministic policy.
 */
class ForwardSimulator {
public:
    /**
     * @brief Simulate the policy from the model initial reservoir volume.
     */
    [[nodiscard]] static std::vector<DispatchStep> simulate(const OptimizationResult& result,
                                                            const DeterministicSeries& series);
};

} // namespace optiflow
