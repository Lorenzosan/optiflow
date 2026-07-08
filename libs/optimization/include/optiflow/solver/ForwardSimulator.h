#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <vector>

namespace optiflow::solver {

/**
 * @brief Forward simulator for applying optimized dispatch decisions.
 */
class ForwardSimulator {
public:
    /**
     * @brief Construct a forward simulator.
     *
     * @param state_grid State grid used by the value function.
     * @param action_grid Action grid used by the solver.
     * @param model Transition and reward model.
     * @param solver_parameters Numerical solver parameters.
     */
    ForwardSimulator(numerics::StateGrid state_grid,
                     numerics::ActionGrid action_grid,
                     model::PumpedStorageModel model,
                     core::SolverParameters solver_parameters);

    /**
     * @brief Simulate dispatch by recomputing the greedy action at each physical state.
     *
     * @param scenario Input scenario.
     * @param value_function Solved value-function table.
     * @return Dispatch trajectory.
     */
    std::vector<core::DispatchStep> simulate_greedy(const core::Scenario& scenario,
                                                    const numerics::ValueFunction& value_function) const;

    /**
     * @brief Simulate dispatch using nearest-grid policy lookup.
     *
     * @param scenario Input scenario.
     * @param policy Solved policy table.
     * @return Dispatch trajectory.
     */
    std::vector<core::DispatchStep> simulate_nearest_policy(const core::Scenario& scenario,
                                                            const numerics::Policy& policy) const;

private:
    numerics::StateGrid state_grid_;
    numerics::ActionGrid action_grid_;
    model::PumpedStorageModel model_;
    core::SolverParameters solver_parameters_;
};

}  // namespace optiflow::solver
