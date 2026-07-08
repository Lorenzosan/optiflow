#pragma once

#include <cstddef>

#include <optiflow/numerics/StateGrid.hpp>
#include <optiflow/numerics/ValueFunction.hpp>

namespace optiflow {

/**
 * @brief Linear interpolation helper for value functions on a StateGrid.
 */
class Interpolator {
public:
    /**
     * @brief Interpolate one value-function row at an arbitrary reservoir volume.
     *
     * The value is clamped to the grid bounds within numerical tolerance.
     */
    [[nodiscard]] static double interpolate(const StateGrid& grid,
                                            const ValueFunction& value_function,
                                            std::size_t time_index,
                                            double reservoir_volume_m3);
};

} // namespace optiflow
