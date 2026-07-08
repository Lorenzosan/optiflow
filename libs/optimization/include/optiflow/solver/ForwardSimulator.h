#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"

#include <span>
#include <vector>

namespace optiflow {

/** Policy lookup strategy for continuous physical states. */
enum class PolicyLookupMode {
  NearestGridPoint,
};

/** Result of applying a solved policy forward from an initial state. */
struct SimulationResult final {
  std::vector<DispatchStep> steps;
  double total_profit_eur{};
  State final_state{};
};

/** Applies a policy to produce a dispatch trajectory. */
class ForwardSimulator final {
public:
  ForwardSimulator(PumpedStorageModel model,
                   StateGrid state_grid,
                   PolicyLookupMode lookup_mode = PolicyLookupMode::NearestGridPoint);

  [[nodiscard]] auto simulate(State initial_state,
                              std::span<const Exogenous> exogenous,
                              const Policy& policy) const -> SimulationResult;

private:
  [[nodiscard]] auto lookup_action(std::size_t time_index, State state, const Policy& policy) const -> Action;

  PumpedStorageModel m_model;
  StateGrid m_state_grid;
  PolicyLookupMode m_lookup_mode;
};

}  // namespace optiflow
