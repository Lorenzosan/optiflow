#include "optiflow/numerics/StateGrid.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace optiflow::numerics {

namespace {

void validate_axis(double min_value, double max_value, std::size_t points, const char* axis_name) {
    if (!std::isfinite(min_value) || !std::isfinite(max_value)) {
        throw std::invalid_argument(std::string(axis_name) + " grid bounds must be finite");
    }
    if (points == 0) {
        throw std::invalid_argument(std::string(axis_name) + " grid must contain at least one point");
    }
    if (min_value > max_value) {
        throw std::invalid_argument(std::string(axis_name) + " grid minimum exceeds maximum");
    }
    if (points > 1 && min_value == max_value) {
        throw std::invalid_argument(std::string(axis_name) + " grid has multiple points but zero width");
    }
}

}  // namespace

StateGrid::StateGrid(double reservoir_min_volume,
                     double reservoir_max_volume,
                     std::size_t reservoir_points,
                     double battery_min_soc,
                     double battery_max_soc,
                     std::size_t battery_points)
    : reservoir_min_volume_(reservoir_min_volume),
      reservoir_max_volume_(reservoir_max_volume),
      reservoir_points_(reservoir_points),
      battery_min_soc_(battery_min_soc),
      battery_max_soc_(battery_max_soc),
      battery_points_(battery_points) {
    validate_axis(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_, "reservoir");
    validate_axis(battery_min_soc_, battery_max_soc_, battery_points_, "battery");
}

StateGrid StateGrid::from_parameters(const core::ModelParameters& model_parameters,
                                     const core::SolverParameters& solver_parameters) {
    return StateGrid(model_parameters.reservoir_min_volume,
                     model_parameters.reservoir_max_volume,
                     solver_parameters.reservoir_volume_grid_points,
                     model_parameters.battery_min_soc,
                     model_parameters.battery_max_soc,
                     solver_parameters.battery_soc_grid_points);
}

core::State StateGrid::state_at(StateIndex index) const {
    if (index.reservoir_index >= reservoir_points_ || index.battery_index >= battery_points_) {
        throw std::out_of_range("state index is outside the state grid");
    }
    return core::State(reservoir_volume_at(index.reservoir_index), battery_soc_at(index.battery_index));
}

double StateGrid::reservoir_volume_at(std::size_t index) const {
    return coordinate_at(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_, index);
}

double StateGrid::battery_soc_at(std::size_t index) const {
    return coordinate_at(battery_min_soc_, battery_max_soc_, battery_points_, index);
}

GridCell StateGrid::cell_for(core::State state) const {
    return GridCell(bracket_for(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_,
                                state.reservoir_volume, "reservoir"),
                    bracket_for(battery_min_soc_, battery_max_soc_, battery_points_,
                                state.battery_soc, "battery"));
}

StateIndex StateGrid::nearest_index(core::State state) const {
    return StateIndex(nearest_axis_index(reservoir_min_volume_, reservoir_max_volume_, reservoir_points_,
                                         state.reservoir_volume),
                      nearest_axis_index(battery_min_soc_, battery_max_soc_, battery_points_,
                                         state.battery_soc));
}

std::size_t StateGrid::reservoir_size() const {
    return reservoir_points_;
}

std::size_t StateGrid::battery_size() const {
    return battery_points_;
}

bool StateGrid::contains(core::State state) const {
    return state.reservoir_volume >= reservoir_min_volume_ &&
           state.reservoir_volume <= reservoir_max_volume_ &&
           state.battery_soc >= battery_min_soc_ &&
           state.battery_soc <= battery_max_soc_;
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

GridBracket StateGrid::bracket_for(double min_value,
                                   double max_value,
                                   std::size_t points,
                                   double value,
                                   const char* axis_name) {
    if (value < min_value || value > max_value) {
        throw std::out_of_range(std::string(axis_name) + " value is outside grid bounds");
    }
    if (points == 1) {
        return GridBracket(0, 0, 0.0);
    }

    const double scaled = (value - min_value) * static_cast<double>(points - 1) / (max_value - min_value);
    const double clamped = std::clamp(scaled, 0.0, static_cast<double>(points - 1));
    const auto lower = static_cast<std::size_t>(std::floor(clamped));
    const std::size_t upper = std::min(lower + 1, points - 1);
    const double weight = clamped - static_cast<double>(lower);
    return GridBracket(lower, upper, weight);
}

std::size_t StateGrid::nearest_axis_index(double min_value,
                                          double max_value,
                                          std::size_t points,
                                          double value) {
    if (points == 1) {
        return 0;
    }
    const double scaled = (value - min_value) * static_cast<double>(points - 1) / (max_value - min_value);
    const double rounded = std::round(std::clamp(scaled, 0.0, static_cast<double>(points - 1)));
    return static_cast<std::size_t>(rounded);
}

}  // namespace optiflow::numerics
