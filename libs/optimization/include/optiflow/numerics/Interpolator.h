#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"

#include <cstddef>

namespace optiflow {

/** Stateless bilinear interpolation over the two-dimensional state grid. */
class BilinearInterpolator final {
public:
  [[nodiscard]] static auto interpolate(const StateGrid& grid,
                                        const ValueFunction& value_function,
                                        std::size_t time_index,
                                        State state) -> double;
};

}  // namespace optiflow
