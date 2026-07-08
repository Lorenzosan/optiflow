#include "optiflow/solver/ForwardSimulator.h"

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
    const auto action = lookup_action(time_index, state, policy);
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
                                     const Policy& policy) const -> Action {
  switch (m_lookup_mode) {
    case PolicyLookupMode::NearestGridPoint: {
      const auto index = m_state_grid.nearest_index(state);
      const auto action = policy.action_at(time_index, index);

      if (!action.has_value()) {
        throw std::runtime_error{"no policy action for nearest grid state"};
      }

      return *action;
    }
  }

  throw std::runtime_error{"unsupported policy lookup mode"};
}

}  // namespace optiflow
