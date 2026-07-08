#include <optiflow/numerics/StateGrid.hpp>

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace optiflow {
namespace {

constexpr double duplicate_tolerance = 1.0e-9;

void validate_grid_arguments(double min_value, double max_value, std::size_t num_points) {
    if (num_points < 2) {
        throw std::invalid_argument("state grid must contain at least two points");
    }
    if (max_value <= min_value) {
        throw std::invalid_argument("state grid max must exceed min");
    }
}

std::vector<double> make_uniform_values(double min_value, double max_value, std::size_t num_points) {
    std::vector<double> values;
    values.reserve(num_points);
    const double step = (max_value - min_value) / static_cast<double>(num_points - 1);
    for (std::size_t index = 0; index < num_points; ++index) {
        values.push_back(min_value + static_cast<double>(index) * step);
    }
    values.back() = max_value;
    return values;
}

void append_validated_anchors(std::vector<double>& values,
                              double min_value,
                              double max_value,
                              const std::vector<double>& anchors) {
    constexpr double bound_tolerance = 1.0e-9;
    values.reserve(values.size() + anchors.size());

    for (const double anchor : anchors) {
        if (!std::isfinite(anchor)) {
            throw std::invalid_argument("state grid anchor must be finite");
        }
        if (anchor < min_value - bound_tolerance || anchor > max_value + bound_tolerance) {
            throw std::invalid_argument("state grid anchor is outside grid bounds");
        }
        values.push_back(std::clamp(anchor, min_value, max_value));
    }
}

void sort_and_deduplicate(std::vector<double>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end(), [](double lhs, double rhs) {
                     return std::abs(lhs - rhs) <= duplicate_tolerance;
                 }),
                 values.end());
}

} // namespace

StateGrid::StateGrid(double min_value, double max_value, std::size_t num_points) {
    validate_grid_arguments(min_value, max_value, num_points);
    values_ = make_uniform_values(min_value, max_value, num_points);
}

StateGrid::StateGrid(double min_value,
                     double max_value,
                     std::size_t num_points,
                     const std::vector<double>& anchors) {
    validate_grid_arguments(min_value, max_value, num_points);
    values_ = make_uniform_values(min_value, max_value, num_points);
    append_validated_anchors(values_, min_value, max_value, anchors);
    sort_and_deduplicate(values_);
    values_.front() = min_value;
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
