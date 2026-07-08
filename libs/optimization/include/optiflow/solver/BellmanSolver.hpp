#pragma once

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/solver/OptimizationResult.hpp>
#include <optiflow/solver/SolverTypes.hpp>

namespace optiflow {

/**
 * @brief Deterministic Bellman solver for the reservoir-only pumped-storage model.
 */
class BellmanSolver {
public:
    /**
     * @brief Construct a solver with a discretization configuration.
     */
    explicit BellmanSolver(BellmanSolverConfig config);

    /**
     * @brief Solve the deterministic finite-horizon dispatch problem.
     *
     * @throws std::invalid_argument if the input series or model parameters are invalid.
     */
    [[nodiscard]] OptimizationResult solve(const DeterministicSeries& series,
                                           const ModelParameters& parameters) const;

private:
    BellmanSolverConfig config_;
};

} // namespace optiflow
