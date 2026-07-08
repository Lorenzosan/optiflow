#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"
#include "optiflow/numerics/StateGrid.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace optiflow::numerics {

/**
 * @brief Tabulated best action for each time and grid state.
 */
class Policy {
public:
    /**
     * @brief Construct an empty policy table.
     *
     * @param horizon_size Number of decision time steps.
     * @param state_grid State grid.
     */
    Policy(std::size_t horizon_size, const StateGrid& state_grid);

    /**
     * @brief Store the best action for a time and grid state.
     *
     * @param time_index Time index in [0, horizon_size - 1].
     * @param state_index State-grid index.
     * @param action Best action.
     */
    void set(std::size_t time_index, StateIndex state_index, core::Action action);

    /**
     * @brief Return the stored action for a time and grid state.
     *
     * @param time_index Time index in [0, horizon_size - 1].
     * @param state_index State-grid index.
     * @return Stored action.
     * @throws std::runtime_error if the entry was not set.
     */
    const core::Action& get(std::size_t time_index, StateIndex state_index) const;

    /**
     * @brief Return whether an entry has been set.
     *
     * @param time_index Time index in [0, horizon_size - 1].
     * @param state_index State-grid index.
     * @return True if an action is stored.
     */
    bool has_action(std::size_t time_index, StateIndex state_index) const;

    /**
     * @brief Return the number of decision time steps.
     *
     * @return Horizon size.
     */
    std::size_t horizon_size() const;

private:
    std::size_t horizon_size_;
    std::size_t reservoir_size_;
    std::size_t battery_size_;
    std::vector<std::optional<core::Action>> actions_;

    /**
     * @brief Compute the flat storage offset for a policy entry.
     */
    std::size_t offset(std::size_t time_index, StateIndex state_index) const;
};

}  // namespace optiflow::numerics
