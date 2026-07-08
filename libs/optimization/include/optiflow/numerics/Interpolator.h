#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <cstddef>

namespace optiflow::numerics {

/**
 * @brief Interpolation utilities for the tabulated value function.
 */
class Interpolator {
public:
    /**
     * @brief Bilinearly interpolate the value function at an off-grid state.
     *
     * @param value_function Value-function table.
     * @param state_grid State grid used by the value function.
     * @param time_index Time index.
     * @param state Physical state.
     * @return Interpolated value.
     */
    static double bilinear(const ValueFunction& value_function,
                           const StateGrid& state_grid,
                           std::size_t time_index,
                           core::State state);
};

}  // namespace optiflow::numerics
