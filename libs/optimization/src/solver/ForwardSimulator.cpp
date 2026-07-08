#include "optiflow/solver/ForwardSimulator.h"

#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace optiflow {

ForwardSimulator::ForwardSimulator(PumpedStorageModel model,
                                   StateGrid state_grid,
                                   const PolicyLookupMode lookup_mode)
    : m_model(std::move(model)),
      m_state_grid(std::move(state_grid)),
      m_lookup_mode(lookup_mode) {}

auto ForwardSimulator::simulate(const State initial_state,
                                const std::span<const Exogenous> exogenous,
                                const Policy& policy) const -> SimulationResult {
  if (exogenous.size() > policy.time_count()) {
    throw std::invalid_argument{"policy horizon is shorter than simulation horizon"};
  }

  if (!m_model.is_state_feasible(initial_state)) {
    throw std::invalid_argument{"initial state is infeasible"};
  }

  SimulationResult result{};
  result.steps.reserve(exogenous.size());

  auto state = initial_state;
  auto cumulative_profit = 0.0;

  for (std::size_t time_index = 0U; time_index < exogenous.size(); ++time_index) {
    const auto action = lookup_action(time_index, state, exogenous[time_index], policy);
    const auto outcome = m_model.evaluate(state, action, exogenous[time_index]);

    if (!outcome.feasible) {
      throw std::runtime_error{"policy produced an infeasible transition during forward simulation"};
    }

    cumulative_profit += outcome.reward_eur;

    result.steps.push_back(DispatchStep{
        .time_index = time_index,
        .state = state,
        .action = action,
        .exogenous = exogenous[time_index],
        .outcome = outcome,
        .cumulative_profit_eur = cumulative_profit,
    });

    state = outcome.next_state;
  }

  result.total_profit_eur = cumulative_profit;
  result.final_state = state;

  return result;
}

auto ForwardSimulator::lookup_action(const std::size_t time_index,
                                     const State state,
                                     const Exogenous exogenous,
                                     const Policy& policy) const -> Action {
  switch (m_lookup_mode) {
    case PolicyLookupMode::NearestGridPoint: {
      const auto action = lookup_nearest_policy_action(time_index, state, policy);
      if (m_model.evaluate(state, action, exogenous).feasible) {
        return action;
      }

      return lookup_nearest_feasible_policy_action(time_index, state, exogenous, policy);
    }
  }

  throw std::runtime_error{"unsupported policy lookup mode"};
}

auto ForwardSimulator::lookup_nearest_policy_action(const std::size_t time_index,
                                                    const State state,
                                                    const Policy& policy) const -> Action {
  const auto index = m_state_grid.nearest_index(state);
  const auto action = policy.action_at(time_index, index);

  if (!action.has_value()) {
    throw std::runtime_error{"no policy action for nearest grid state"};
  }

  return *action;
}

auto ForwardSimulator::lookup_nearest_feasible_policy_action(const std::size_t time_index,
                                                             const State state,
                                                             const Exogenous exogenous,
                                                             const Policy& policy) const -> Action {
  auto best_distance = std::numeric_limits<double>::infinity();
  auto best_action = Action{};
  auto found = false;

  for (std::size_t reservoir_index = 0U; reservoir_index < m_state_grid.reservoir_size(); ++reservoir_index) {
    for (std::size_t battery_index = 0U; battery_index < m_state_grid.battery_size(); ++battery_index) {
      const auto state_index = StateIndex{
          .reservoir_index = reservoir_index,
          .battery_index = battery_index,
      };
      const auto action = policy.action_at(time_index, state_index);
      if (!action.has_value()) {
        continue;
      }

      const auto outcome = m_model.evaluate(state, *action, exogenous);
      if (!outcome.feasible) {
        continue;
      }

      const auto grid_state = m_state_grid.state_at(state_index);
      const auto reservoir_span = m_state_grid.reservoir_points().back() - m_state_grid.reservoir_points().front();
      const auto battery_span = m_state_grid.battery_points().back() - m_state_grid.battery_points().front();
      const auto normalized_reservoir_distance = reservoir_span > 0.0
          ? (state.reservoir_volume_m3 - grid_state.reservoir_volume_m3) / reservoir_span
          : 0.0;
      const auto normalized_battery_distance = battery_span > 0.0
          ? (state.battery_soc_mwh - grid_state.battery_soc_mwh) / battery_span
          : 0.0;
      const auto distance = normalized_reservoir_distance * normalized_reservoir_distance
                          + normalized_battery_distance * normalized_battery_distance;

      if (!found || distance < best_distance) {
        found = true;
        best_distance = distance;
        best_action = *action;
      }
    }
  }

  if (!found) {
    throw std::runtime_error{"no feasible policy action for current physical state"};
  }

  return best_action;
}

}  // namespace optiflow
