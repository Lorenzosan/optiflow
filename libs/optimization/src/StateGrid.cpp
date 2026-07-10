#include "optiflow/numerics/StateGrid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace optiflow::numerics {

namespace {

constexpr double tolerance = 1.0e-9;

void validate_axis(double min_value, double max_value, std::size_t points) {
    if (!std::isfinite(min_value) || !std::isfinite(max_value)) {
        throw std::invalid_argument("reservoir grid bounds must be finite");
    }
    if (points == 0) {
        throw std::invalid_argument("reservoir grid must contain at least one point");
    }
    if (min_value > max_value) {
        throw std::invalid_argument("reservoir grid minimum exceeds maximum");
    }
    if (points > 1 && min_value == max_value) {
        throw std::invalid_argument("reservoir grid has multiple points but zero width");
    }
}

}  // namespace

StateGrid::StateGrid(double reservoir_min_volume,
                     double reservoir_max_volume,
                     std::size_t reservoir_points)
    : reservoir_min_volume_(reservoir_min_volume),
      reservoir_max_volume_(reservoir_max_volume),
      reservoir_points_(reservoir_points) {
    validate_axis(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_);
}

StateGrid StateGrid::from_parameters(const core::ModelParameters& model_parameters,
                                     const core::SolverParameters& solver_parameters) {
    return StateGrid(model_parameters.reservoir_min_volume,
                     model_parameters.reservoir_max_volume,
                     solver_parameters.reservoir_volume_grid_points);
}

core::State StateGrid::state_at(StateIndex index) const {
    return core::State(reservoir_volume_at(index.reservoir_index));
}

double StateGrid::reservoir_volume_at(std::size_t index) const {
    return coordinate_at(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_, index);
}

GridBracket StateGrid::bracket_for(core::State state) const {
    return bracket_for_value(reservoir_min_volume_,
                             reservoir_max_volume_,
                             reservoir_points_,
                             state.reservoir_volume,
                             "reservoir");
}

StateIndex StateGrid::nearest_index(core::State state) const {
    return StateIndex(nearest_axis_index(reservoir_min_volume_,
                                         reservoir_max_volume_,
                                         reservoir_points_,
                                         state.reservoir_volume));
}

std::size_t StateGrid::reservoir_size() const { return reservoir_points_; }

bool StateGrid::contains(core::State state) const {
    return state.reservoir_volume >= reservoir_min_volume_ - tolerance &&
           state.reservoir_volume <= reservoir_max_volume_ + tolerance;
}

double StateGrid::coordinate_at(double min_value,
                                double max_value,
                                std::size_t points,
                                std::size_t index) {
    if (index >= points) {
        throw std::out_of_range("grid coordinate index is out of range");
    }
    if (points == 1) {
        return min_value;
    }
    const double step = (max_value - min_value) / static_cast<double>(points - 1);
    return min_value + step * static_cast<double>(index);
}

GridBracket StateGrid::bracket_for_value(double min_value,
                                         double max_value,
                                         std::size_t points,
                                         double value,
                                         const char* axis_name) {
    if (value < min_value - tolerance || value > max_value + tolerance) {
        throw std::out_of_range(std::string(axis_name) + " value is outside grid bounds");
    }
    if (points == 1) {
        return GridBracket(0, 0, 0.0);
    }

    const double scaled = (value - min_value) * static_cast<double>(points - 1) /
                          (max_value - min_value);
    const double clamped = std::clamp(scaled, 0.0, static_cast<double>(points - 1));
    const auto lower = static_cast<std::size_t>(std::floor(clamped));
    const std::size_t upper = std::min(lower + 1, points - 1);
    return GridBracket(lower, upper, clamped - static_cast<double>(lower));
}

std::size_t StateGrid::nearest_axis_index(double min_value,
                                          double max_value,
                                          std::size_t points,
                                          double value) {
    if (points == 1) {
        return 0;
    }
    const double scaled = (value - min_value) * static_cast<double>(points - 1) /
                          (max_value - min_value);
    return static_cast<std::size_t>(
        std::round(std::clamp(scaled, 0.0, static_cast<double>(points - 1))));
}

}  // namespace optiflow::numerics
