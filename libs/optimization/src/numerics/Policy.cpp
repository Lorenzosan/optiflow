#include "optiflow/numerics/Policy.h"

#include <stdexcept>

namespace optiflow {

Policy::Policy(const std::size_t time_count,
               const std::size_t reservoir_count,
               const std::size_t battery_count)
    : m_time_count(time_count),
      m_reservoir_count(reservoir_count),
      m_battery_count(battery_count),
      m_actions(time_count * reservoir_count * battery_count) {
  if (m_time_count == 0U || m_reservoir_count == 0U || m_battery_count == 0U) {
    throw std::invalid_argument{"policy dimensions must be non-zero"};
  }
}

auto Policy::time_count() const noexcept -> std::size_t {
  return m_time_count;
}

auto Policy::reservoir_count() const noexcept -> std::size_t {
  return m_reservoir_count;
}

auto Policy::battery_count() const noexcept -> std::size_t {
  return m_battery_count;
}

auto Policy::action_at(const std::size_t time_index,
                       const StateIndex state_index) const -> std::optional<Action> {
  return m_actions.at(flat_index(time_index, state_index));
}

void Policy::set_action(const std::size_t time_index,
                        const StateIndex state_index,
                        const Action action) {
  m_actions.at(flat_index(time_index, state_index)) = action;
}

auto Policy::flat_index(const std::size_t time_index, const StateIndex state_index) const -> std::size_t {
  if (time_index >= m_time_count
      || state_index.reservoir_index >= m_reservoir_count
      || state_index.battery_index >= m_battery_count) {
    throw std::out_of_range{"policy index out of range"};
  }

  return (time_index * m_reservoir_count * m_battery_count)
      + (state_index.reservoir_index * m_battery_count)
      + state_index.battery_index;
}

}  // namespace optiflow
