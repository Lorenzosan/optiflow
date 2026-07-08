#include "optiflow/stochastic/StochasticBellmanSolver.h"

#include "optiflow/numerics/Interpolator.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace optiflow {
namespace {

constexpr double probability_tolerance = 1.0e-9;

void validate_process(const StochasticExogenousProcess& process) {
  if (process.empty()) {
    throw std::invalid_argument{"stochastic process must contain at least one time step"};
  }

  for (const auto& distribution : process) {
    if (distribution.empty()) {
      throw std::invalid_argument{"each stochastic stage must contain at least one realization"};
    }

    auto probability_sum = 0.0;
    for (const auto& realization : distribution) {
      if (realization.probability < 0.0) {
        throw std::invalid_argument{"stochastic realization probability cannot be negative"};
      }
      probability_sum += realization.probability;
    }

    if (std::abs(probability_sum - 1.0) > probability_tolerance) {
      throw std::invalid_argument{"stochastic stage probabilities must sum to one"};
    }
  }
}

}  // namespace

StochasticBellmanSolver::StochasticBellmanSolver(PumpedStorageModel model,
                                                 StateGrid state_grid,
                                                 ActionGrid action_grid,
                                                 OptimizationConfig config)
    : m_model{std::move(model)},
      m_state_grid{std::move(state_grid)},
      m_action_grid{std::move(action_grid)},
      m_config{config} {}

[[nodiscard]] auto StochasticBellmanSolver::solve(const StochasticExogenousProcess& process) const
    -> OptimizationResult {
  validate_process(process);

  const auto horizon = process.size();
  auto value_function = ValueFunction{
      horizon + 1U,
      m_state_grid.reservoir_size(),
      m_state_grid.battery_size(),
      0.0,
  };
  auto policy = Policy{horizon, m_state_grid.reservoir_size(), m_state_grid.battery_size()};

  for (std::size_t reservoir_index = 0; reservoir_index < m_state_grid.reservoir_size(); ++reservoir_index) {
    for (std::size_t battery_index = 0; battery_index < m_state_grid.battery_size(); ++battery_index) {
      const auto state_index = StateIndex{.reservoir_index = reservoir_index, .battery_index = battery_index};
      value_function(horizon, state_index) = m_model.terminal_value(m_state_grid.state_at(state_index));
    }
  }

  for (std::size_t reverse_time = horizon; reverse_time > 0U; --reverse_time) {
    const auto time_index = reverse_time - 1U;

    for (std::size_t reservoir_index = 0; reservoir_index < m_state_grid.reservoir_size(); ++reservoir_index) {
      for (std::size_t battery_index = 0; battery_index < m_state_grid.battery_size(); ++battery_index) {
        const auto state_index = StateIndex{.reservoir_index = reservoir_index, .battery_index = battery_index};
        const auto state = m_state_grid.state_at(state_index);
        const auto [best_value, best_action] = best_action_at(time_index, state, process, value_function);
        value_function(time_index, state_index) = best_value;
        policy.set_action(time_index, state_index, best_action);
      }
    }
  }

  return OptimizationResult{.value_function = std::move(value_function), .policy = std::move(policy)};
}

[[nodiscard]] auto StochasticBellmanSolver::best_action_at(std::size_t time_index,
                                                           State state,
                                                           const StochasticExogenousProcess& process,
                                                           const ValueFunction& value_function) const
    -> std::pair<double, Action> {
  auto best_value = -std::numeric_limits<double>::infinity();
  auto best_action = Action{};

  for (const auto& action : m_action_grid.actions()) {
    if (m_config.forbid_simultaneous_pump_and_turbine &&
        action.pump_flow_m3_s > 0.0 && action.turbine_flow_m3_s > 0.0) {
      continue;
    }
    if (m_config.forbid_simultaneous_charge_and_discharge &&
        action.battery_charge_mw > 0.0 && action.battery_discharge_mw > 0.0) {
      continue;
    }
    if (!m_model.is_action_structurally_feasible(action)) {
      continue;
    }

    auto expected_value = 0.0;
    auto feasible_for_all_realizations = true;

    for (const auto& realization : process[time_index]) {
      const auto outcome = m_model.evaluate(state, action, realization.value);
      if (!outcome.feasible || !m_state_grid.contains(outcome.next_state)) {
        feasible_for_all_realizations = false;
        break;
      }

      const auto continuation_value = BilinearInterpolator::interpolate(
          m_state_grid,
          value_function,
          time_index + 1U,
          outcome.next_state);
      expected_value += realization.probability *
                        (outcome.reward_eur + m_config.discount_factor * continuation_value);
    }

    if (feasible_for_all_realizations && expected_value > best_value) {
      best_value = expected_value;
      best_action = action;
    }
  }

  if (!std::isfinite(best_value)) {
    return {m_config.infeasible_value, Action{}};
  }

  return {best_value, best_action};
}

}  // namespace optiflow
