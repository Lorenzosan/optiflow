#include "optiflow/numerics/Interpolator.h"

#include <stdexcept>

namespace optiflow {

auto BilinearInterpolator::interpolate(const StateGrid& grid,
                                       const ValueFunction& value_function,
                                       const std::size_t time_index,
                                       const State state) -> double {
  if (!grid.contains(state)) {
    throw std::out_of_range{"state is outside interpolation grid"};
  }

  const auto bracket = grid.bracket(state);
  const auto rw = bracket.reservoir.upper_weight;
  const auto bw = bracket.battery.upper_weight;

  const auto r0 = bracket.reservoir.lower_index;
  const auto r1 = bracket.reservoir.upper_index;
  const auto b0 = bracket.battery.lower_index;
  const auto b1 = bracket.battery.upper_index;

  const auto v00 = value_function(time_index, StateIndex{.reservoir_index = r0, .battery_index = b0});
  const auto v10 = value_function(time_index, StateIndex{.reservoir_index = r1, .battery_index = b0});
  const auto v01 = value_function(time_index, StateIndex{.reservoir_index = r0, .battery_index = b1});
  const auto v11 = value_function(time_index, StateIndex{.reservoir_index = r1, .battery_index = b1});

  return ((1.0 - rw) * (1.0 - bw) * v00)
       + (rw * (1.0 - bw) * v10)
       + ((1.0 - rw) * bw * v01)
       + (rw * bw * v11);
}

}  // namespace optiflow
