#include <optiflow/numerics/StateGrid.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optiflow {

StateGrid::StateGrid(double min_value, double max_value, std::size_t num_points) {
    if (num_points < 2) {
        throw std::invalid_argument("state grid must contain at least two points");
    }
    if (max_value <= min_value) {
        throw std::invalid_argument("state grid max must exceed min");
    }

    values_.reserve(num_points);
    const double step = (max_value - min_value) / static_cast<double>(num_points - 1);
    for (std::size_t index = 0; index < num_points; ++index) {
        values_.push_back(min_value + static_cast<double>(index) * step);
    }
    values_.back() = max_value;
}

const std::vector<double>& StateGrid::values() const noexcept { return values_; }

std::size_t StateGrid::size() const noexcept { return values_.size(); }

double StateGrid::min() const noexcept { return values_.front(); }

double StateGrid::max() const noexcept { return values_.back(); }

double StateGrid::at(std::size_t index) const { return values_.at(index); }

std::size_t StateGrid::nearest_index(double value) const {
    if (value <= min()) {
        return 0;
    }
    if (value >= max()) {
        return values_.size() - 1;
    }

    const auto upper = std::lower_bound(values_.begin(), values_.end(), value);
    const auto upper_index = static_cast<std::size_t>(std::distance(values_.begin(), upper));
    const auto lower_index = upper_index - 1;

    const double lower_distance = std::abs(value - values_.at(lower_index));
    const double upper_distance = std::abs(values_.at(upper_index) - value);
    return lower_distance <= upper_distance ? lower_index : upper_index;
}

} // namespace optiflow
