#pragma once

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/numerics/Policy.hpp>
#include <optiflow/numerics/StateGrid.hpp>
#include <optiflow/numerics/ValueFunction.hpp>

namespace optiflow {

/**
 * @brief Result of deterministic Bellman optimization.
 */
struct OptimizationResult {
    StateGrid state_grid;
    ValueFunction value_function;
    Policy policy;
    ModelParameters parameters;
    double objective_value_eur{};
};

} // namespace optiflow
