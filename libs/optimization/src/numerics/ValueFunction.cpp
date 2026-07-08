#include "optiflow/numerics/ValueFunction.h"

#include <stdexcept>

namespace optiflow {

ValueFunction::ValueFunction(const std::size_t time_count,
                             const std::size_t reservoir_count,
                             const std::size_t battery_count,
                             const double initial_value)
    : m_time_count(time_count),
      m_reservoir_count(reservoir_count),
      m_battery_count(battery_count),
      m_values(time_count * reservoir_count * battery_count, initial_value) {
  if (m_time_count == 0U || m_reservoir_count == 0U || m_battery_count == 0U) {
    throw std::invalid_argument{"value function dimensions must be non-zero"};
  }
}

auto ValueFunction::time_count() const noexcept -> std::size_t {
  return m_time_count;
}

auto ValueFunction::reservoir_count() const noexcept -> std::size_t {
  return m_reservoir_count;
}

auto ValueFunction::battery_count() const noexcept -> std::size_t {
  return m_battery_count;
}

auto ValueFunction::operator()(const std::size_t time_index, const StateIndex state_index) const -> double {
  return m_values.at(flat_index(time_index, state_index));
}

auto ValueFunction::operator()(const std::size_t time_index, const StateIndex state_index) -> double& {
  return m_values.at(flat_index(time_index, state_index));
}

auto ValueFunction::flat_index(const std::size_t time_index, const StateIndex state_index) const -> std::size_t {
  if (time_index >= m_time_count
      || state_index.reservoir_index >= m_reservoir_count
      || state_index.battery_index >= m_battery_count) {
    throw std::out_of_range{"value function index out of range"};
  }

  return (time_index * m_reservoir_count * m_battery_count)
      + (state_index.reservoir_index * m_battery_count)
      + state_index.battery_index;
}

}  // namespace optiflow
