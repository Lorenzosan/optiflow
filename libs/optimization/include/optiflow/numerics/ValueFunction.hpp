#pragma once

#include <cstddef>
#include <vector>

namespace optiflow {

/**
 * @brief Dense value-function table indexed by time and reservoir state.
 */
class ValueFunction {
public:
    /**
     * @brief Construct a value-function table.
     *
     * @param time_steps Number of decision time steps.
     * @param state_count Number of reservoir grid states.
     */
    ValueFunction(std::size_t time_steps, std::size_t state_count);

    /**
     * @brief Return the value at a time and state index.
     */
    [[nodiscard]] double at(std::size_t time_index, std::size_t state_index) const;

    /**
     * @brief Set the value at a time and state index.
     */
    void set(std::size_t time_index, std::size_t state_index, double value);

    /**
     * @brief Return number of rows, including the terminal row.
     */
    [[nodiscard]] std::size_t time_count() const noexcept;

    /**
     * @brief Return number of state columns.
     */
    [[nodiscard]] std::size_t state_count() const noexcept;

private:
    std::size_t time_count_{};
    std::size_t state_count_{};
    std::vector<double> values_;
};

} // namespace optiflow
