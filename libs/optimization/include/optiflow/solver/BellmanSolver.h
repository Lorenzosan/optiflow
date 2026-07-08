#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <span>
#include <utility>

namespace optiflow {

/** Output of a Bellman solve. */
struct OptimizationResult final {
  ValueFunction value_function;
  Policy policy;
};

/** Deterministic finite-horizon Bellman dynamic-programming solver. */
class BellmanSolver final {
public:
  BellmanSolver(PumpedStorageModel model,
                StateGrid state_grid,
                ActionGrid action_grid,
                OptimizationConfig config = {});

  /** Solve the backward induction problem for the supplied exogenous time series. */
  [[nodiscard]] auto solve(std::span<const Exogenous> exogenous) const -> OptimizationResult;

private:
  [[nodiscard]] auto best_action_at(std::size_t time_index,
                                    State state,
                                    Exogenous exogenous,
                                    const ValueFunction& value_function) const -> std::pair<double, Action>;

  [[nodiscard]] auto action_allowed_by_config(Action action) const noexcept -> bool;

  PumpedStorageModel m_model;
  StateGrid m_state_grid;
  ActionGrid m_action_grid;
  OptimizationConfig m_config;
};

}  // namespace optiflow
