#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/core/StorageTypes.h"

#include <vector>

namespace optiflow::runner {

/**
 * @brief Complete result produced by one optimization run.
 */
struct OptimizationResult {
    std::vector<core::DispatchStep> dispatch; ///< Forward dispatch trajectory.
    double cumulative_profit; ///< Final cumulative profit.
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
     * @return Optimization result containing dispatch and cumulative profit.
     */
    OptimizationResult run(const core::ScenarioBundle& bundle) const;
};

}  // namespace optiflow::runner
