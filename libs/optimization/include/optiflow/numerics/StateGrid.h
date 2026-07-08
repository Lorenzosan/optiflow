#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/GridTypes.h"

#include <cstddef>

namespace optiflow::numerics {

/**
 * @brief Uniform 2D grid for reservoir volume and battery state of charge.
 */
class StateGrid {
public:
    /**
     * @brief Construct a uniform 2D state grid.
     *
     * @param reservoir_min_volume Minimum reservoir volume.
     * @param reservoir_max_volume Maximum reservoir volume.
     * @param reservoir_points Number of reservoir grid points.
     * @param battery_min_soc Minimum battery state of charge.
     * @param battery_max_soc Maximum battery state of charge.
     * @param battery_points Number of battery grid points.
     */
    StateGrid(double reservoir_min_volume,
              double reservoir_max_volume,
              std::size_t reservoir_points,
              double battery_min_soc,
              double battery_max_soc,
              std::size_t battery_points);

    /**
     * @brief Build a state grid from model and solver parameters.
     *
     * @param model_parameters Physical model parameters.
     * @param solver_parameters Numerical solver parameters.
     * @return State grid.
     */
    static StateGrid from_parameters(const core::ModelParameters& model_parameters,
                                     const core::SolverParameters& solver_parameters);

    /**
     * @brief Return the physical state at a grid index.
     *
     * @param index Grid index.
     * @return Physical state.
     */
    core::State state_at(StateIndex index) const;

    /**
     * @brief Return a reservoir grid coordinate.
     *
     * @param index Reservoir index.
     * @return Reservoir volume.
     */
    double reservoir_volume_at(std::size_t index) const;

    /**
     * @brief Return a battery grid coordinate.
     *
     * @param index Battery index.
     * @return Battery state of charge.
     */
    double battery_soc_at(std::size_t index) const;

    /**
     * @brief Return the interpolation cell containing a physical state.
     *
     * @param state Physical state.
     * @return Bilinear interpolation cell.
     */
    GridCell cell_for(core::State state) const;

    /**
     * @brief Return the nearest grid index to a physical state.
     *
     * @param state Physical state.
     * @return Nearest state-grid index.
     */
    StateIndex nearest_index(core::State state) const;

    /**
     * @brief Return the number of reservoir grid points.
     *
     * @return Reservoir grid size.
     */
    std::size_t reservoir_size() const;

    /**
     * @brief Return the number of battery grid points.
     *
     * @return Battery grid size.
     */
    std::size_t battery_size() const;

    /**
     * @brief Check whether a physical state is inside the grid bounds.
     *
     * @param state Physical state.
     * @return True if the state is inside bounds.
     */
    bool contains(core::State state) const;

private:
    double reservoir_min_volume_;
    double reservoir_max_volume_;
    std::size_t reservoir_points_;
    double battery_min_soc_;
    double battery_max_soc_;
    std::size_t battery_points_;

    /**
     * @brief Return a one-dimensional uniform-grid coordinate.
     */
    static double coordinate_at(double min_value,
                                double max_value,
                                std::size_t points,
                                std::size_t index);

    /**
     * @brief Return a one-dimensional interpolation bracket.
     */
    static GridBracket bracket_for(double min_value,
                                   double max_value,
                                   std::size_t points,
                                   double value,
                                   const char* axis_name);

    /**
     * @brief Return the nearest one-dimensional grid index.
     */
    static std::size_t nearest_axis_index(double min_value,
                                          double max_value,
                                          std::size_t points,
                                          double value);
};

}  // namespace optiflow::numerics
