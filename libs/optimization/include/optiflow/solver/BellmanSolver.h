#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

namespace optiflow::solver {

/**
 * @brief Result of a Bellman dynamic-programming solve.
 */
struct BellmanResult {
    numerics::ValueFunction value_function; ///< Solved value-function table.
    numerics::Policy policy; ///< Solved best-action policy.

    /**
     * @brief Construct a Bellman solve result.
     *
     * @param value_function Value-function table.
     * @param policy Best-action lookup table.
     */
    BellmanResult(numerics::ValueFunction value_function, numerics::Policy policy);
};

/**
 * @brief Deterministic finite-horizon Bellman dynamic-programming solver.
 */
class BellmanSolver {
public:
    /**
     * @brief Construct a Bellman solver.
     *
     * @param state_grid State grid.
     * @param action_grid Action grid.
     * @param model Transition and reward model.
     * @param solver_parameters Numerical solver parameters.
     */
    BellmanSolver(numerics::StateGrid state_grid,
                  numerics::ActionGrid action_grid,
                  model::PumpedStorageModel model,
                  core::SolverParameters solver_parameters);

    /**
     * @brief Solve a scenario by backward induction.
     *
     * @param scenario Input scenario.
     * @return Value function and tabulated policy.
     */
    BellmanResult solve(const core::Scenario& scenario) const;

private:
    numerics::StateGrid state_grid_;
    numerics::ActionGrid action_grid_;
    model::PumpedStorageModel model_;
    core::SolverParameters solver_parameters_;
};

}  // namespace optiflow::solver
