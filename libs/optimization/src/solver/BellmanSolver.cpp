#include "optiflow/solver/BellmanSolver.h"

#include "optiflow/numerics/Interpolator.h"

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace optiflow {
namespace {

constexpr double activity_tolerance = 1.0e-12;

[[nodiscard]] auto is_finite(const double value) noexcept -> bool {
  return std::isfinite(value);
}

}  // namespace

BellmanSolver::BellmanSolver(PumpedStorageModel model,
                             StateGrid state_grid,
                             ActionGrid action_grid,
                             OptimizationConfig config)
    : m_model(std::move(model)),
      m_state_grid(std::move(state_grid)),
      m_action_grid(std::move(action_grid)),
      m_config(config) {
  if (m_config.discount_factor < 0.0 || m_config.discount_factor > 1.0) {
    throw std::invalid_argument{"discount_factor must be in [0, 1]"};
  }
}

auto BellmanSolver::solve(const std::span<const Exogenous> exogenous) const -> OptimizationResult {
  if (exogenous.empty()) {
    throw std::invalid_argument{"exogenous time series must not be empty"};
  }

  const auto horizon = exogenous.size();

  ValueFunction value_function{
      horizon + 1U,
      m_state_grid.reservoir_size(),
      m_state_grid.battery_size(),
      m_config.infeasible_value,
  };

  Policy policy{
      horizon,
      m_state_grid.reservoir_size(),
      m_state_grid.battery_size(),
  };

  for (std::size_t flat = 0U; flat < m_state_grid.state_count(); ++flat) {
    const auto state_index = m_state_grid.index_from_flat(flat);
    const auto state = m_state_grid.state_at(state_index);
    value_function(horizon, state_index) = m_model.terminal_value(state);
  }

  for (std::size_t time_index = horizon; time_index-- > 0U;) {
    for (std::size_t flat = 0U; flat < m_state_grid.state_count(); ++flat) {
      const auto state_index = m_state_grid.index_from_flat(flat);
      const auto state = m_state_grid.state_at(state_index);
      const auto [best_value, best_action] = best_action_at(time_index, state, exogenous[time_index], value_function);

      value_function(time_index, state_index) = best_value;

      if (is_finite(best_value)) {
        policy.set_action(time_index, state_index, best_action);
      }
    }
  }

  return OptimizationResult{
      .value_function = std::move(value_function),
      .policy = std::move(policy),
  };
}

auto BellmanSolver::best_action_at(const std::size_t time_index,
                                   const State state,
                                   const Exogenous exogenous,
                                   const ValueFunction& value_function) const -> std::pair<double, Action> {
  auto best_value = -std::numeric_limits<double>::infinity();
  Action best_action{};

  for (const auto action : m_action_grid.actions()) {
    if (!action_allowed_by_config(action)) {
      continue;
    }

    const auto outcome = m_model.evaluate(state, action, exogenous);

    if (!outcome.feasible || !m_state_grid.contains(outcome.next_state)) {
      continue;
    }

    const auto continuation_value = BilinearInterpolator::interpolate(
        m_state_grid,
        value_function,
        time_index + 1U,
        outcome.next_state);

    if (!is_finite(continuation_value)) {
      continue;
    }

    const auto candidate_value = outcome.reward_eur + m_config.discount_factor * continuation_value;

    if (candidate_value > best_value) {
      best_value = candidate_value;
      best_action = action;
    }
  }

  return std::pair{best_value, best_action};
}

auto BellmanSolver::action_allowed_by_config(const Action action) const noexcept -> bool {
  if (m_config.forbid_simultaneous_pump_and_turbine
      && action.pump_flow_m3_s > activity_tolerance
      && action.turbine_flow_m3_s > activity_tolerance) {
    return false;
  }

  if (m_config.forbid_simultaneous_charge_and_discharge
      && action.battery_charge_mw > activity_tolerance
      && action.battery_discharge_mw > activity_tolerance) {
    return false;
  }

  return true;
}

}  // namespace optiflow
