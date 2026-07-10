#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <vector>

namespace optiflow::solver {

class ForwardSimulator {
public:
    ForwardSimulator(numerics::StateGrid state_grid,
                     numerics::ActionGrid action_grid,
                     model::PumpedStorageModel model,
                     core::SolverParameters solver_parameters);

    std::vector<core::DispatchStep> simulate_from_value_function(
        const core::Scenario& scenario,
        const numerics::ValueFunction& value_function) const;

    std::vector<core::DispatchStep> simulate_nearest_policy(
        const core::Scenario& scenario,
        const numerics::Policy& policy) const;

private:
    numerics::StateGrid state_grid_;
    numerics::ActionGrid action_grid_;
    model::PumpedStorageModel model_;
    core::SolverParameters solver_parameters_;
};

}  // namespace optiflow::solver
