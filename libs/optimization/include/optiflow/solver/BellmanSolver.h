#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

namespace optiflow::solver {

struct BellmanResult {
    numerics::ValueFunction value_function;
    numerics::Policy policy;

    BellmanResult(numerics::ValueFunction value_function, numerics::Policy policy);
};

class BellmanSolver {
public:
    BellmanSolver(numerics::StateGrid state_grid,
                  numerics::ActionGrid action_grid,
                  model::PumpedStorageModel model,
                  core::SolverParameters solver_parameters);

    BellmanResult solve(const core::Scenario& scenario) const;

private:
    numerics::StateGrid state_grid_;
    numerics::ActionGrid action_grid_;
    model::PumpedStorageModel model_;
    core::SolverParameters solver_parameters_;
};

}  // namespace optiflow::solver
