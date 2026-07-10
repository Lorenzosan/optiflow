#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"

#include <cstddef>

namespace optiflow::numerics {

/** @brief Uniform one-dimensional grid for upper-reservoir volume. */
class StateGrid {
public:
    StateGrid(double reservoir_min_volume,
              double reservoir_max_volume,
              std::size_t reservoir_points);

    static StateGrid from_parameters(const core::ModelParameters& model_parameters,
                                     const core::SolverParameters& solver_parameters);

    core::State state_at(StateIndex index) const;
    double reservoir_volume_at(std::size_t index) const;
    GridBracket bracket_for(core::State state) const;
    StateIndex nearest_index(core::State state) const;
    std::size_t reservoir_size() const;
    bool contains(core::State state) const;

private:
    double reservoir_min_volume_;
    double reservoir_max_volume_;
    std::size_t reservoir_points_;

    static double coordinate_at(double min_value,
                                double max_value,
                                std::size_t points,
                                std::size_t index);
    static GridBracket bracket_for_value(double min_value,
                                         double max_value,
                                         std::size_t points,
                                         double value,
                                         const char* axis_name);
    static std::size_t nearest_axis_index(double min_value,
                                          double max_value,
                                          std::size_t points,
                                          double value);
};

}  // namespace optiflow::numerics
