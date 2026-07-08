#pragma once

#include <optiflow/solver/DeterministicProblem.hpp>
#include <optiflow/solver/OptimizationResult.hpp>

namespace optiflow {

/**
 * @brief Deterministic Bellman solver for the reservoir-only pumped-storage model.
 *
 * The solver is independent from CSV files, CLI arguments, HTTP payloads, and
 * persistence. Callers must provide a validated in-memory DeterministicProblem.
 */
class BellmanSolver {
public:
    /**
     * @brief Solve the deterministic finite-horizon dispatch problem.
     *
     * @throws std::invalid_argument if the problem is invalid.
     * @throws std::runtime_error if no feasible action is found during recursion.
     */
    [[nodiscard]] OptimizationResult solve(const DeterministicProblem& problem) const;
};

} // namespace optiflow
