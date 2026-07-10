#include "optiflow/numerics/Interpolator.h"

#include <cmath>
#include <limits>

namespace optiflow::numerics {

namespace {

constexpr double tolerance = 1.0e-12;

double weighted_value(double weight, double value) {
    if (std::abs(weight) <= tolerance) {
        return 0.0;
    }
    if (!std::isfinite(value)) {
        return value;
    }
    return weight * value;
}

}  // namespace

double Interpolator::linear(const ValueFunction& value_function,
                            const StateGrid& state_grid,
                            std::size_t time_index,
                            core::State state) {
    const GridBracket bracket = state_grid.bracket_for(state);
    const double lower = weighted_value(
        1.0 - bracket.upper_weight,
        value_function.get(time_index, StateIndex(bracket.lower_index)));
    const double upper = weighted_value(
        bracket.upper_weight,
        value_function.get(time_index, StateIndex(bracket.upper_index)));
    if (!std::isfinite(lower) || !std::isfinite(upper)) {
        return -std::numeric_limits<double>::infinity();
    }
    return lower + upper;
}

}  // namespace optiflow::numerics
