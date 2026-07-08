#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/stochastic/StochasticTypes.h"

namespace optiflow {

/**
 * Stochastic finite-horizon Bellman solver for stagewise discrete uncertainty.
 *
 * The implementation assumes the action is chosen after observing the physical
 * state but before the current exogenous realization is known. Therefore the
 * policy depends on time and physical state, not on price or inflow. If the
 * exogenous process is observed before dispatch, price and inflow regimes should
 * be added to the state or modeled with a different policy structure.
 */
class StochasticBellmanSolver final {
public:
  StochasticBellmanSolver(PumpedStorageModel model,
                          StateGrid state_grid,
                          ActionGrid action_grid,
                          OptimizationConfig config = {});

  [[nodiscard]] auto solve(const StochasticExogenousProcess& process) const -> OptimizationResult;

private:
  [[nodiscard]] auto best_action_at(std::size_t time_index,
                                    State state,
                                    const StochasticExogenousProcess& process,
                                    const ValueFunction& value_function) const
      -> std::pair<double, Action>;

  PumpedStorageModel m_model;
  StateGrid m_state_grid;
  ActionGrid m_action_grid;
  OptimizationConfig m_config;
};

}  // namespace optiflow
