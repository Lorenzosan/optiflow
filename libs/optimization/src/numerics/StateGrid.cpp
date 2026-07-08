#include "optiflow/numerics/StateGrid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace optiflow {

StateGrid::StateGrid(std::vector<double> reservoir_points_m3,
                     std::vector<double> battery_points_mwh)
    : m_reservoir_points_m3(std::move(reservoir_points_m3)),
      m_battery_points_mwh(std::move(battery_points_mwh)) {
  validate_axis(m_reservoir_points_m3, "reservoir grid");
  validate_axis(m_battery_points_mwh, "battery grid");
}

auto StateGrid::reservoir_size() const noexcept -> std::size_t {
  return m_reservoir_points_m3.size();
}

auto StateGrid::battery_size() const noexcept -> std::size_t {
  return m_battery_points_mwh.size();
}

auto StateGrid::state_count() const noexcept -> std::size_t {
  return reservoir_size() * battery_size();
}

auto StateGrid::reservoir_points() const noexcept -> std::span<const double> {
  return m_reservoir_points_m3;
}

auto StateGrid::battery_points() const noexcept -> std::span<const double> {
  return m_battery_points_mwh;
}

auto StateGrid::state_at(const StateIndex index) const -> State {
  if (index.reservoir_index >= reservoir_size() || index.battery_index >= battery_size()) {
    throw std::out_of_range{"state index out of range"};
  }

  return State{
      .reservoir_volume_m3 = m_reservoir_points_m3[index.reservoir_index],
      .battery_soc_mwh = m_battery_points_mwh[index.battery_index],
  };
}

auto StateGrid::flat_index(const StateIndex index) const -> std::size_t {
  if (index.reservoir_index >= reservoir_size() || index.battery_index >= battery_size()) {
    throw std::out_of_range{"state index out of range"};
  }

  return (index.reservoir_index * battery_size()) + index.battery_index;
}

auto StateGrid::index_from_flat(const std::size_t flat_index_value) const -> StateIndex {
  if (flat_index_value >= state_count()) {
    throw std::out_of_range{"flat state index out of range"};
  }

  return StateIndex{
      .reservoir_index = flat_index_value / battery_size(),
      .battery_index = flat_index_value % battery_size(),
  };
}

auto StateGrid::nearest_index(const State state) const -> StateIndex {
  return StateIndex{
      .reservoir_index = nearest_axis_index(m_reservoir_points_m3, state.reservoir_volume_m3),
      .battery_index = nearest_axis_index(m_battery_points_mwh, state.battery_soc_mwh),
  };
}

auto StateGrid::bracket(const State state) const -> GridBracket2D {
  return GridBracket2D{
      .reservoir = bracket_axis(m_reservoir_points_m3, state.reservoir_volume_m3),
      .battery = bracket_axis(m_battery_points_mwh, state.battery_soc_mwh),
  };
}

auto StateGrid::contains(const State state) const noexcept -> bool {
  return state.reservoir_volume_m3 >= m_reservoir_points_m3.front()
      && state.reservoir_volume_m3 <= m_reservoir_points_m3.back()
      && state.battery_soc_mwh >= m_battery_points_mwh.front()
      && state.battery_soc_mwh <= m_battery_points_mwh.back();
}

auto StateGrid::nearest_axis_index(const std::span<const double> axis, const double value) -> std::size_t {
  const auto upper = std::lower_bound(axis.begin(), axis.end(), value);

  if (upper == axis.begin()) {
    return 0U;
  }

  if (upper == axis.end()) {
    return axis.size() - 1U;
  }

  const auto upper_index = static_cast<std::size_t>(std::distance(axis.begin(), upper));
  const auto lower_index = upper_index - 1U;

  const auto lower_distance = std::abs(value - axis[lower_index]);
  const auto upper_distance = std::abs(axis[upper_index] - value);

  return lower_distance <= upper_distance ? lower_index : upper_index;
}

auto StateGrid::bracket_axis(const std::span<const double> axis, const double value) -> GridBracket1D {
  if (axis.size() == 1U) {
    return GridBracket1D{.lower_index = 0U, .upper_index = 0U, .upper_weight = 0.0};
  }

  if (value <= axis.front()) {
    return GridBracket1D{.lower_index = 0U, .upper_index = 0U, .upper_weight = 0.0};
  }

  if (value >= axis.back()) {
    const auto last = axis.size() - 1U;
    return GridBracket1D{.lower_index = last, .upper_index = last, .upper_weight = 0.0};
  }

  const auto upper = std::lower_bound(axis.begin(), axis.end(), value);
  const auto upper_index = static_cast<std::size_t>(std::distance(axis.begin(), upper));
  const auto lower_index = upper_index - 1U;

  if (*upper == value) {
    return GridBracket1D{.lower_index = upper_index, .upper_index = upper_index, .upper_weight = 0.0};
  }

  const auto lower_value = axis[lower_index];
  const auto upper_value = axis[upper_index];
  const auto upper_weight = (value - lower_value) / (upper_value - lower_value);

  return GridBracket1D{
      .lower_index = lower_index,
      .upper_index = upper_index,
      .upper_weight = upper_weight,
  };
}

void StateGrid::validate_axis(const std::span<const double> axis, const char* const name) {
  if (axis.empty()) {
    throw std::invalid_argument{std::string{name} + " must not be empty"};
  }

  const auto all_finite = std::all_of(axis.begin(), axis.end(), [](const double value) {
    return std::isfinite(value);
  });

  if (!all_finite) {
    throw std::invalid_argument{std::string{name} + " must contain finite values"};
  }

  const auto strictly_increasing = std::adjacent_find(axis.begin(), axis.end(), [](const double lhs, const double rhs) {
    return lhs >= rhs;
  }) == axis.end();

  if (!strictly_increasing) {
    throw std::invalid_argument{std::string{name} + " must be strictly increasing"};
  }
}

}  // namespace optiflow
