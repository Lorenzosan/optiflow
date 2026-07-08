#pragma once

#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <span>
#include <vector>

namespace optiflow {

/** Axis specification used to build a Cartesian action grid. */
struct ActionAxes final {
  std::vector<double> turbine_flow_m3_s;
  std::vector<double> spill_flow_m3_s;
  std::vector<double> pump_flow_m3_s;
  std::vector<double> battery_charge_mw;
  std::vector<double> battery_discharge_mw;
};

/** Collection of candidate actions considered by the Bellman solver. */
class ActionGrid final {
public:
  explicit ActionGrid(std::vector<Action> actions);

  [[nodiscard]] auto size() const noexcept -> std::size_t;
  [[nodiscard]] auto actions() const noexcept -> std::span<const Action>;
  [[nodiscard]] auto operator[](std::size_t index) const -> const Action&;

  /** Build a Cartesian action grid from axes. Keep axes small. */
  [[nodiscard]] static auto from_axes(const ActionAxes& axes) -> ActionGrid;

private:
  std::vector<Action> m_actions;
};

}  // namespace optiflow
