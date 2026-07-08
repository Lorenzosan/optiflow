#pragma once

#include <cstddef>

namespace optiflow {

/** Two-dimensional integer index into the state grid. */
struct StateIndex final {
  std::size_t reservoir_index{};
  std::size_t battery_index{};

  [[nodiscard]] friend constexpr auto operator==(const StateIndex&, const StateIndex&) -> bool = default;
};

/** Lower and upper grid corners surrounding a physical state. */
struct GridCell final {
  StateIndex lower{};
  StateIndex upper{};
};

/** One-dimensional interpolation bracket. */
struct GridBracket1D final {
  std::size_t lower_index{};
  std::size_t upper_index{};
  double upper_weight{};
};

/** Two-dimensional interpolation bracket for reservoir and battery axes. */
struct GridBracket2D final {
  GridBracket1D reservoir{};
  GridBracket1D battery{};
};

}  // namespace optiflow
