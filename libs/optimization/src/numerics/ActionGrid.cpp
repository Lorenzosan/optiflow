#include "optiflow/numerics/ActionGrid.h"

#include <stdexcept>
#include <utility>

namespace optiflow {

ActionGrid::ActionGrid(std::vector<Action> actions)
    : m_actions(std::move(actions)) {
  if (m_actions.empty()) {
    throw std::invalid_argument{"action grid must not be empty"};
  }
}

auto ActionGrid::size() const noexcept -> std::size_t {
  return m_actions.size();
}

auto ActionGrid::actions() const noexcept -> std::span<const Action> {
  return m_actions;
}

auto ActionGrid::operator[](const std::size_t index) const -> const Action& {
  if (index >= m_actions.size()) {
    throw std::out_of_range{"action index out of range"};
  }

  return m_actions[index];
}

auto ActionGrid::from_axes(const ActionAxes& axes) -> ActionGrid {
  if (axes.turbine_flow_m3_s.empty()
      || axes.spill_flow_m3_s.empty()
      || axes.pump_flow_m3_s.empty()
      || axes.battery_charge_mw.empty()
      || axes.battery_discharge_mw.empty()) {
    throw std::invalid_argument{"all action axes must be non-empty"};
  }

  std::vector<Action> actions;
  actions.reserve(axes.turbine_flow_m3_s.size()
                  * axes.spill_flow_m3_s.size()
                  * axes.pump_flow_m3_s.size()
                  * axes.battery_charge_mw.size()
                  * axes.battery_discharge_mw.size());

  for (const auto turbine_flow : axes.turbine_flow_m3_s) {
    for (const auto spill_flow : axes.spill_flow_m3_s) {
      for (const auto pump_flow : axes.pump_flow_m3_s) {
        for (const auto battery_charge : axes.battery_charge_mw) {
          for (const auto battery_discharge : axes.battery_discharge_mw) {
            actions.push_back(Action{
                .turbine_flow_m3_s = turbine_flow,
                .spill_flow_m3_s = spill_flow,
                .pump_flow_m3_s = pump_flow,
                .battery_charge_mw = battery_charge,
                .battery_discharge_mw = battery_discharge,
            });
          }
        }
      }
    }
  }

  return ActionGrid{std::move(actions)};
}

}  // namespace optiflow
