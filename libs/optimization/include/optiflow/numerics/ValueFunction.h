#pragma once

#include "optiflow/numerics/GridTypes.h"
#include "optiflow/numerics/StateGrid.h"

#include <cstddef>
#include <vector>

namespace optiflow::numerics {

/**
 * @brief Tabulated value function V[t, reservoir_index, battery_index].
 */
class ValueFunction {
public:
    /**
     * @brief Construct a value-function table.
     *
     * @param horizon_size Number of decision time steps.
     * @param state_grid State grid.
     */
    ValueFunction(std::size_t horizon_size, const StateGrid& state_grid);

    /**
     * @brief Return a value-function entry.
     *
     * @param time_index Time index in [0, horizon_size].
     * @param state_index State-grid index.
     * @return Value-function entry.
     */
    double get(std::size_t time_index, StateIndex state_index) const;

    /**
     * @brief Set a value-function entry.
     *
     * @param time_index Time index in [0, horizon_size].
     * @param state_index State-grid index.
     * @param value Value to store.
     */
    void set(std::size_t time_index, StateIndex state_index, double value);

    /**
     * @brief Return the number of decision time steps.
     *
     * @return Horizon size.
     */
    std::size_t horizon_size() const;

    /**
     * @brief Return the number of reservoir grid points.
     *
     * @return Reservoir grid size.
     */
    std::size_t reservoir_size() const;

    /**
     * @brief Return the number of battery grid points.
     *
     * @return Battery grid size.
     */
    std::size_t battery_size() const;

private:
    std::size_t horizon_size_;
    std::size_t reservoir_size_;
    std::size_t battery_size_;
    std::vector<double> values_;

    /**
     * @brief Compute the flat storage offset for a table entry.
     */
    std::size_t offset(std::size_t time_index, StateIndex state_index) const;
};

}  // namespace optiflow::numerics
