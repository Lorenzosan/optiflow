#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <cstddef>

namespace optiflow::numerics {

/** @brief Linear interpolation on the one-dimensional reservoir state grid. */
class Interpolator {
public:
    static double linear(const ValueFunction& value_function,
                         const StateGrid& state_grid,
                         std::size_t time_index,
                         core::State state);
};

}  // namespace optiflow::numerics
