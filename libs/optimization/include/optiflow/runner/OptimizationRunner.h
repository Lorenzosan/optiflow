#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <vector>

namespace optiflow::runner {

/**
 * @brief Diagnostics produced by one optimization run.
 */
struct OptimizationDiagnostics {
    std::size_t horizon_steps; ///< Number of simulated decision steps.
    std::size_t reservoir_grid_points; ///< Number of reservoir-volume state-grid points.
    std::size_t battery_grid_points; ///< Number of battery-SOC state-grid points.
    std::size_t action_count; ///< Number of candidate actions considered at each Bellman state.
    double solve_seconds; ///< Wall-clock time spent in the Bellman solve.
    double simulation_seconds; ///< Wall-clock time spent in forward simulation.
    std::size_t turbine_steps; ///< Number of dispatch steps with positive turbine flow.
    std::size_t pump_steps; ///< Number of dispatch steps with positive pump flow.
    std::size_t spill_steps; ///< Number of dispatch steps with positive spill flow.
    std::size_t battery_charge_steps; ///< Number of dispatch steps with positive battery charging power.
    std::size_t battery_discharge_steps; ///< Number of dispatch steps with positive battery discharging power.
    std::size_t wait_steps; ///< Number of dispatch steps with no active control action.
};

/**
 * @brief Complete result produced by one optimization run.
 */
struct OptimizationResult {
    std::vector<core::DispatchStep> dispatch; ///< Forward dispatch trajectory.
    double cumulative_profit; ///< Final cumulative profit.
    OptimizationDiagnostics diagnostics; ///< Solver and dispatch diagnostics.
};

/**
 * @brief Service-facing wrapper around the Bellman solver and forward simulator.
 */
class OptimizationRunner {
public:
    /**
     * @brief Solve a scenario bundle and simulate the resulting dispatch.
     *
     * @param bundle Scenario, model parameters, terminal parameters, and solver parameters.
     * @return Optimization result containing dispatch, cumulative profit, and diagnostics.
     */
    OptimizationResult run(const core::ScenarioBundle& bundle) const;
};

}  // namespace optiflow::runner
