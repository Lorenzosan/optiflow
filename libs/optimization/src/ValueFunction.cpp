#include "optiflow/numerics/ValueFunction.h"

#include <stdexcept>

namespace optiflow::numerics {

ValueFunction::ValueFunction(std::size_t horizon_size, const StateGrid& state_grid)
    : horizon_size_(horizon_size),
      reservoir_size_(state_grid.reservoir_size()),
      battery_size_(state_grid.battery_size()),
      values_((horizon_size + 1) * state_grid.reservoir_size() * state_grid.battery_size(), 0.0) {
    if (horizon_size_ == 0) {
        throw std::invalid_argument("value function horizon must be positive");
    }
}

double ValueFunction::get(std::size_t time_index, StateIndex state_index) const {
    return values_.at(offset(time_index, state_index));
}

void ValueFunction::set(std::size_t time_index, StateIndex state_index, double value) {
    values_.at(offset(time_index, state_index)) = value;
}

std::size_t ValueFunction::horizon_size() const {
    return horizon_size_;
}

std::size_t ValueFunction::reservoir_size() const {
    return reservoir_size_;
}

std::size_t ValueFunction::battery_size() const {
    return battery_size_;
}

std::size_t ValueFunction::offset(std::size_t time_index, StateIndex state_index) const {
    if (time_index > horizon_size_) {
        throw std::out_of_range("value-function time index is out of range");
    }
    if (state_index.reservoir_index >= reservoir_size_ || state_index.battery_index >= battery_size_) {
        throw std::out_of_range("value-function state index is out of range");
    }
    return (time_index * reservoir_size_ + state_index.reservoir_index) * battery_size_ +
           state_index.battery_index;
}

}  // namespace optiflow::numerics
