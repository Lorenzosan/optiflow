#pragma once

#include <cstddef>

namespace optiflow::numerics {

/**
 * @brief Integer index of one point on the 2D state grid.
 */
struct StateIndex {
    std::size_t reservoir_index; ///< Reservoir-volume grid index.
    std::size_t battery_index; ///< Battery-SOC grid index.

    /**
     * @brief Construct a state-grid index.
     *
     * @param reservoir_index Reservoir-volume grid index.
     * @param battery_index Battery-SOC grid index.
     */
    StateIndex(std::size_t reservoir_index, std::size_t battery_index);
};

/**
 * @brief One-dimensional interpolation bracket.
 */
struct GridBracket {
    std::size_t lower_index; ///< Lower grid index.
    std::size_t upper_index; ///< Upper grid index.
    double upper_weight; ///< Weight assigned to the upper index.

    /**
     * @brief Construct a one-dimensional interpolation bracket.
     *
     * @param lower_index Lower grid index.
     * @param upper_index Upper grid index.
     * @param upper_weight Interpolation weight of the upper index.
     */
    GridBracket(std::size_t lower_index, std::size_t upper_index, double upper_weight);
};

/**
 * @brief Bilinear interpolation cell on the 2D state grid.
 */
struct GridCell {
    GridBracket reservoir_bracket; ///< Reservoir-volume interpolation bracket.
    GridBracket battery_bracket; ///< Battery-SOC interpolation bracket.

    /**
     * @brief Construct a 2D interpolation cell.
     *
     * @param reservoir_bracket Reservoir-volume interpolation bracket.
     * @param battery_bracket Battery-SOC interpolation bracket.
     */
    GridCell(GridBracket reservoir_bracket, GridBracket battery_bracket);
};

}  // namespace optiflow::numerics
