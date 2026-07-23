#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <vector>

namespace optiflow::runner {

struct OptimizationDiagnostics {
    std::size_t horizon_steps;
    std::size_t reservoir_grid_points;
    std::size_t action_count;
    double solve_seconds;
    double simulation_seconds;
    double export_energy_mwh;
    double import_energy_mwh;
    double final_reservoir_volume;
    std::size_t turbine_steps;
    std::size_t pump_steps;
    std::size_t spill_steps;
    std::size_t wait_steps;
};

struct OptimizationResult {
    std::vector<core::DispatchStep> dispatch;
    double net_operating_cashflow;
    OptimizationDiagnostics diagnostics;
};

class OptimizationRunner {
public:
    OptimizationResult run(const core::ScenarioBundle& bundle) const;
};

}  // namespace optiflow::runner
