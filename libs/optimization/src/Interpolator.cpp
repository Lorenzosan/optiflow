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

double combine(double first_weight, double first_value, double second_weight, double second_value) {
    const double first = weighted_value(first_weight, first_value);
    const double second = weighted_value(second_weight, second_value);
    if (!std::isfinite(first) || !std::isfinite(second)) {
        return -std::numeric_limits<double>::infinity();
    }
    return first + second;
}

}  // namespace

double Interpolator::bilinear(const ValueFunction& value_function,
                              const StateGrid& state_grid,
                              std::size_t time_index,
                              core::State state) {
    const GridCell cell = state_grid.cell_for(state);

    const double w_reservoir = cell.reservoir_bracket.upper_weight;
    const double w_battery = cell.battery_bracket.upper_weight;

    const StateIndex ll(cell.reservoir_bracket.lower_index, cell.battery_bracket.lower_index);
    const StateIndex lu(cell.reservoir_bracket.lower_index, cell.battery_bracket.upper_index);
    const StateIndex ul(cell.reservoir_bracket.upper_index, cell.battery_bracket.lower_index);
    const StateIndex uu(cell.reservoir_bracket.upper_index, cell.battery_bracket.upper_index);

    const double v_ll = value_function.get(time_index, ll);
    const double v_lu = value_function.get(time_index, lu);
    const double v_ul = value_function.get(time_index, ul);
    const double v_uu = value_function.get(time_index, uu);

    const double lower_reservoir_value = combine(1.0 - w_battery, v_ll, w_battery, v_lu);
    const double upper_reservoir_value = combine(1.0 - w_battery, v_ul, w_battery, v_uu);
    return combine(1.0 - w_reservoir, lower_reservoir_value, w_reservoir, upper_reservoir_value);
}

}  // namespace optiflow::numerics
