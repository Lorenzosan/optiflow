#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace optiflow {

/** Lookup table storing the best action at each time and grid state. */
class Policy final {
public:
  Policy(std::size_t time_count, std::size_t reservoir_count, std::size_t battery_count);

  [[nodiscard]] auto time_count() const noexcept -> std::size_t;
  [[nodiscard]] auto reservoir_count() const noexcept -> std::size_t;
  [[nodiscard]] auto battery_count() const noexcept -> std::size_t;

  [[nodiscard]] auto action_at(std::size_t time_index,
                               StateIndex state_index) const -> std::optional<Action>;

  void set_action(std::size_t time_index, StateIndex state_index, Action action);

private:
  [[nodiscard]] auto flat_index(std::size_t time_index, StateIndex state_index) const -> std::size_t;

  std::size_t m_time_count{};
  std::size_t m_reservoir_count{};
  std::size_t m_battery_count{};
  std::vector<std::optional<Action>> m_actions;
};

}  // namespace optiflow
