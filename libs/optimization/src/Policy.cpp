#include "optiflow/numerics/Policy.h"

#include <stdexcept>

namespace optiflow::numerics {

Policy::Policy(std::size_t horizon_size, const StateGrid& state_grid)
    : horizon_size_(horizon_size),
      reservoir_size_(state_grid.reservoir_size()),
      actions_(horizon_size * state_grid.reservoir_size()) {
    if (horizon_size_ == 0) {
        throw std::invalid_argument("policy horizon must be positive");
    }
}

void Policy::set(std::size_t time_index, StateIndex state_index, core::Action action) {
    actions_.at(offset(time_index, state_index)) = action;
}

const core::Action& Policy::get(std::size_t time_index, StateIndex state_index) const {
    const std::optional<core::Action>& entry = actions_.at(offset(time_index, state_index));
    if (!entry.has_value()) {
        throw std::runtime_error("policy entry has not been set");
    }
    return *entry;
}

bool Policy::has_action(std::size_t time_index, StateIndex state_index) const {
    return actions_.at(offset(time_index, state_index)).has_value();
}

std::size_t Policy::horizon_size() const { return horizon_size_; }

std::size_t Policy::offset(std::size_t time_index, StateIndex state_index) const {
    if (time_index >= horizon_size_) {
        throw std::out_of_range("policy time index is out of range");
    }
    if (state_index.reservoir_index >= reservoir_size_) {
        throw std::out_of_range("policy state index is out of range");
    }
    return time_index * reservoir_size_ + state_index.reservoir_index;
}

}  // namespace optiflow::numerics
