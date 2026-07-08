#pragma once

#include <cstddef>
#include <vector>

namespace optiflow {

/**
 * @brief Ordered grid for reservoir volume states.
 */
class StateGrid {
public:
    /**
     * @brief Construct a uniform reservoir-volume grid.
     *
     * @throws std::invalid_argument if the bounds or number of points are invalid.
     */
    StateGrid(double min_value, double max_value, std::size_t num_points);

    /**
     * @brief Construct a grid with additional anchor points.
     *
     * The grid starts from a uniform discretization and then inserts important
     * reservoir volumes, such as the initial and target final volumes. This
     * keeps the value function exact at economically important states while
     * preserving a regular base resolution.
     *
     * @throws std::invalid_argument if the bounds, number of points, or anchors are invalid.
     */
    StateGrid(double min_value, double max_value, std::size_t num_points,
              const std::vector<double>& anchors);

    /**
     * @brief Return all grid values in ascending order.
     */
    [[nodiscard]] const std::vector<double>& values() const noexcept;

    /**
     * @brief Return the number of grid points.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return the minimum grid value.
     */
    [[nodiscard]] double min() const noexcept;

    /**
     * @brief Return the maximum grid value.
     */
    [[nodiscard]] double max() const noexcept;

    /**
     * @brief Return the grid value at index.
     *
     * @throws std::out_of_range if index is outside the grid.
     */
    [[nodiscard]] double at(std::size_t index) const;

    /**
     * @brief Return the index of the grid point nearest to value.
     */
    [[nodiscard]] std::size_t nearest_index(double value) const;

private:
    std::vector<double> values_;
};

} // namespace optiflow
