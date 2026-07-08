#pragma once

#include "optiflow/core/StorageTypes.h"

#include <string_view>
#include <vector>

namespace optiflow::demo {

struct OptimizationRequest final {
  std::vector<Exogenous> exogenous;
};

/**
 * Parse the lightweight local optimizer request JSON.
 *
 * Supported shape:
 * {
 *   "exogenous": [
 *     {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0}
 *   ]
 * }
 *
 * An empty body or an empty JSON object requests the built-in deterministic demo scenario.
 */
[[nodiscard]] auto parse_optimization_request_json(std::string_view json) -> OptimizationRequest;

}  // namespace optiflow::demo
