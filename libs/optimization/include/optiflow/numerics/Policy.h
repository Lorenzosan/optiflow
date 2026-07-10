#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"
#include "optiflow/numerics/StateGrid.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace optiflow::numerics {

/** @brief Tabulated best action for each time and reservoir state. */
class Policy {
public:
    Policy(std::size_t horizon_size, const StateGrid& state_grid);

    void set(std::size_t time_index, StateIndex state_index, core::Action action);
    const core::Action& get(std::size_t time_index, StateIndex state_index) const;
    bool has_action(std::size_t time_index, StateIndex state_index) const;
    std::size_t horizon_size() const;

private:
    std::size_t horizon_size_;
    std::size_t reservoir_size_;
    std::vector<std::optional<core::Action>> actions_;

    std::size_t offset(std::size_t time_index, StateIndex state_index) const;
};

}  // namespace optiflow::numerics
