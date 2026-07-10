#pragma once

#include "optiflow/numerics/GridTypes.h"
#include "optiflow/numerics/StateGrid.h"

#include <cstddef>
#include <vector>

namespace optiflow::numerics {

/** @brief Tabulated value function V[time, reservoir_index]. */
class ValueFunction {
public:
    ValueFunction(std::size_t horizon_size, const StateGrid& state_grid);

    double get(std::size_t time_index, StateIndex state_index) const;
    void set(std::size_t time_index, StateIndex state_index, double value);
    std::size_t horizon_size() const;
    std::size_t reservoir_size() const;

private:
    std::size_t horizon_size_;
    std::size_t reservoir_size_;
    std::vector<double> values_;

    std::size_t offset(std::size_t time_index, StateIndex state_index) const;
};

}  // namespace optiflow::numerics
