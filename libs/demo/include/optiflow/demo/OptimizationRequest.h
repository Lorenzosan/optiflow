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
  std::vector<Exogenous> exogenous;
  StochasticExogenousProcess stochastic_process;
};

/**
 * Parse the lightweight local optimizer request JSON.
 *
 * Supported deterministic shape:
 * {
 *   "solver_kind": "deterministic",
 *   "exogenous": [
 *     {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0}
 *   ]
 * }
 *
 * Supported stochastic shape:
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
