#include <optiflow/numerics/Policy.hpp>

#include <stdexcept>

namespace optiflow {

Policy::Policy(std::size_t time_steps, std::size_t state_count)
    : time_count_(time_steps), state_count_(state_count), actions_(time_count_ * state_count_) {
    if (time_steps == 0) {
        throw std::invalid_argument("policy time count must be positive");
    }
    if (state_count == 0) {
        throw std::invalid_argument("policy state count must be positive");
    }
}

const HydroAction& Policy::at(std::size_t time_index, std::size_t state_index) const {
    if (time_index >= time_count_ || state_index >= state_count_) {
        throw std::out_of_range("policy index out of range");
    }
    return actions_.at(time_index * state_count_ + state_index);
}

void Policy::set(std::size_t time_index, std::size_t state_index, const HydroAction& action) {
    if (time_index >= time_count_ || state_index >= state_count_) {
        throw std::out_of_range("policy index out of range");
    }
    actions_.at(time_index * state_count_ + state_index) = action;
}

std::size_t Policy::time_count() const noexcept { return time_count_; }

std::size_t Policy::state_count() const noexcept { return state_count_; }

} // namespace optiflow
