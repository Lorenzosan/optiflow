#pragma once

#include "optiflow/numerics/GridTypes.h"

#include <cstddef>
#include <vector>

namespace optiflow {

/** Flat storage for V[time][reservoir][battery]. */
class ValueFunction final {
public:
  ValueFunction(std::size_t time_count,
                std::size_t reservoir_count,
                std::size_t battery_count,
                double initial_value = 0.0);

  [[nodiscard]] auto time_count() const noexcept -> std::size_t;
  [[nodiscard]] auto reservoir_count() const noexcept -> std::size_t;
  [[nodiscard]] auto battery_count() const noexcept -> std::size_t;

  [[nodiscard]] auto operator()(std::size_t time_index, StateIndex state_index) const -> double;
  auto operator()(std::size_t time_index, StateIndex state_index) -> double&;

private:
  [[nodiscard]] auto flat_index(std::size_t time_index, StateIndex state_index) const -> std::size_t;

  std::size_t m_time_count{};
  std::size_t m_reservoir_count{};
  std::size_t m_battery_count{};
  std::vector<double> m_values;
};

}  // namespace optiflow
