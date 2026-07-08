#include <optiflow/numerics/Interpolator.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optiflow {

double Interpolator::interpolate(const StateGrid& grid, const ValueFunction& value_function,
                                 std::size_t time_index, double reservoir_volume_m3) {
    constexpr double tolerance = 1.0e-9;

    if (value_function.state_count() != grid.size()) {
        throw std::invalid_argument("value function state count does not match grid size");
    }

    if (reservoir_volume_m3 <= grid.min() + tolerance) {
        return value_function.at(time_index, 0);
    }
    if (reservoir_volume_m3 >= grid.max() - tolerance) {
        return value_function.at(time_index, grid.size() - 1);
    }

    const auto& values = grid.values();
    const auto upper = std::lower_bound(values.begin(), values.end(), reservoir_volume_m3);
    if (upper == values.begin() || upper == values.end()) {
        throw std::out_of_range("interpolation volume outside grid");
    }

    const auto upper_index = static_cast<std::size_t>(std::distance(values.begin(), upper));
    const auto lower_index = upper_index - 1;

    const double lower_volume = values.at(lower_index);
    const double upper_volume = values.at(upper_index);
    const double weight = (reservoir_volume_m3 - lower_volume) / (upper_volume - lower_volume);

    const double lower_value = value_function.at(time_index, lower_index);
    const double upper_value = value_function.at(time_index, upper_index);
    return lower_value * (1.0 - weight) + upper_value * weight;
}

} // namespace optiflow
