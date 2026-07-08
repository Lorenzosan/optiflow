#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"

#include <cstddef>
#include <span>
#include <vector>

namespace optiflow {

/** Tensor-product grid for reservoir volume and battery state of charge. */
class StateGrid final {
public:
  /**
   * Construct a state grid from sorted reservoir and battery points.
   * Throws std::invalid_argument if either axis is empty or not sorted.
   */
  StateGrid(std::vector<double> reservoir_points_m3, std::vector<double> battery_points_mwh);

  [[nodiscard]] auto reservoir_size() const noexcept -> std::size_t;
  [[nodiscard]] auto battery_size() const noexcept -> std::size_t;
  [[nodiscard]] auto state_count() const noexcept -> std::size_t;

  [[nodiscard]] auto reservoir_points() const noexcept -> std::span<const double>;
  [[nodiscard]] auto battery_points() const noexcept -> std::span<const double>;

  [[nodiscard]] auto state_at(StateIndex index) const -> State;
  [[nodiscard]] auto flat_index(StateIndex index) const -> std::size_t;
  [[nodiscard]] auto index_from_flat(std::size_t flat_index) const -> StateIndex;

  [[nodiscard]] auto nearest_index(State state) const -> StateIndex;
  [[nodiscard]] auto bracket(State state) const -> GridBracket2D;

  [[nodiscard]] auto contains(State state) const noexcept -> bool;

private:
  [[nodiscard]] static auto nearest_axis_index(std::span<const double> axis, double value) -> std::size_t;
  [[nodiscard]] static auto bracket_axis(std::span<const double> axis, double value) -> GridBracket1D;
  static void validate_axis(std::span<const double> axis, const char* name);

  std::vector<double> m_reservoir_points_m3;
  std::vector<double> m_battery_points_mwh;
};

}  // namespace optiflow
