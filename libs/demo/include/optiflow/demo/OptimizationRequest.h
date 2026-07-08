#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/stochastic/StochasticTypes.h"

#include <string_view>
#include <vector>

namespace optiflow::demo {

enum class RequestSolverKind {
  Deterministic,
  Stochastic,
};

struct OptimizationRequest final {
  RequestSolverKind solver_kind{RequestSolverKind::Deterministic};
  ModelParameters parameters{};
  State initial_state{};
  OptimizationConfig config{};
  std::vector<Exogenous> exogenous;
  StochasticExogenousProcess stochastic_process;
};

/**
 * Parse the lightweight local optimizer request JSON.
 *
 * Supported deterministic shape:
 * {
 *   "solver_kind": "deterministic",
 *   "initial_state": {"reservoir_volume_m3": 50000000.0, "battery_soc_mwh": 25.0},
 *   "parameters": {
 *     "timestep_hours": 1.0,
 *     "hydro": {"max_reservoir_volume_m3": 100000000.0, "max_turbine_flow_m3_s": 150.0},
 *     "battery": {"enabled": true, "capacity_mwh": 50.0}
 *   },
 *   "optimization_config": {"discount_factor": 1.0},
 *   "exogenous": [
 *     {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0}
 *   ]
 * }
 *
 * Supported stochastic shape uses the same parameter and initial_state fields plus:
 * {
 *   "solver_kind": "stochastic",
 *   "stochastic_process": [
 *     {"time_index": 0, "realizations": [
 *       {"probability": 0.5, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0}
 *     ]}
 *   ]
 * }
 *
 * An empty body or an empty JSON object requests the built-in deterministic demo scenario.
 */
[[nodiscard]] auto parse_optimization_request_json(std::string_view json) -> OptimizationRequest;

}  // namespace optiflow::demo
