#pragma once

#include "optiflow/core/StorageTypes.h"

#include <vector>

namespace optiflow {

/** One possible exogenous realization at a time step, with probability mass. */
struct WeightedExogenous final {
  Exogenous value{};
  double probability{};
};

/** Discrete distribution of exogenous inputs for one time step. */
using StageDistribution = std::vector<WeightedExogenous>;

/** Stagewise discrete uncertainty process over a finite horizon. */
using StochasticExogenousProcess = std::vector<StageDistribution>;

}  // namespace optiflow
