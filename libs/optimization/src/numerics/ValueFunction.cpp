#include <optiflow/numerics/ValueFunction.hpp>

#include <stdexcept>

namespace optiflow {

ValueFunction::ValueFunction(std::size_t time_steps, std::size_t state_count)
    : time_count_(time_steps + 1), state_count_(state_count), values_(time_count_ * state_count_, 0.0) {
    if (state_count == 0) {
        throw std::invalid_argument("value function state count must be positive");
    }
}

double ValueFunction::at(std::size_t time_index, std::size_t state_index) const {
    if (time_index >= time_count_ || state_index >= state_count_) {
        throw std::out_of_range("value function index out of range");
    }
    return values_.at(time_index * state_count_ + state_index);
}

void ValueFunction::set(std::size_t time_index, std::size_t state_index, double value) {
    if (time_index >= time_count_ || state_index >= state_count_) {
        throw std::out_of_range("value function index out of range");
    }
    values_.at(time_index * state_count_ + state_index) = value;
}

std::size_t ValueFunction::time_count() const noexcept { return time_count_; }

std::size_t ValueFunction::state_count() const noexcept { return state_count_; }

} // namespace optiflow
