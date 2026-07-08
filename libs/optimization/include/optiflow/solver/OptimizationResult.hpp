#pragma once

#include <optiflow/numerics/Policy.hpp>
#include <optiflow/numerics/StateGrid.hpp>
#include <optiflow/numerics/ValueFunction.hpp>
#include <optiflow/solver/DeterministicProblem.hpp>

namespace optiflow {

/**
 * @brief Result of deterministic Bellman optimization.
 */
struct OptimizationResult {
    StateGrid state_grid;
    ValueFunction value_function;
    Policy policy;
    DeterministicProblem problem;
    double objective_value_eur{};
};

} // namespace optiflow
