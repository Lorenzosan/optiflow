#pragma once

#include "optiflow/core/StorageTypes.h"

namespace optiflow {

/**
 * Simplified physical-economic model for pumped storage and an optional battery.
 *
 * The model owns parameters and evaluates one-step transitions. It does not know
 * about grids, value functions, policies, services, databases, or serialization.
 */
class PumpedStorageModel final {
public:
  explicit PumpedStorageModel(ModelParameters parameters);

  [[nodiscard]] auto parameters() const noexcept -> const ModelParameters&;

  /** Evaluate one transition and reward. Infeasible actions return feasible false. */
  [[nodiscard]] auto evaluate(State state, Action action, Exogenous exogenous) const -> Outcome;

  /** Approximate continuation value at the end of the finite horizon. */
  [[nodiscard]] auto terminal_value(State state) const -> double;

  [[nodiscard]] auto is_state_feasible(State state) const noexcept -> bool;
  [[nodiscard]] auto is_action_structurally_feasible(Action action) const noexcept -> bool;

private:
  [[nodiscard]] auto turbine_power_mw(double flow_m3_s) const noexcept -> double;
  [[nodiscard]] auto pump_power_mw(double flow_m3_s) const noexcept -> double;
  [[nodiscard]] auto timestep_seconds() const noexcept -> double;

  ModelParameters m_parameters;
};

}  // namespace optiflow
