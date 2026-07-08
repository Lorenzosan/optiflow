#pragma once

#include <cstddef>
#include <vector>

#include <optiflow/core/StorageTypes.hpp>

namespace optiflow {

/**
 * @brief Deterministic policy table indexed by time and reservoir grid state.
 */
class Policy {
public:
    /**
     * @brief Construct a policy table.
     */
    Policy(std::size_t time_steps, std::size_t state_count);

    /**
     * @brief Return the action at a time and state index.
     */
    [[nodiscard]] const HydroAction& at(std::size_t time_index, std::size_t state_index) const;

    /**
     * @brief Store the action at a time and state index.
     */
    void set(std::size_t time_index, std::size_t state_index, const HydroAction& action);

    /**
     * @brief Return number of decision time rows.
     */
    [[nodiscard]] std::size_t time_count() const noexcept;

    /**
     * @brief Return number of state columns.
     */
    [[nodiscard]] std::size_t state_count() const noexcept;

private:
    std::size_t time_count_{};
    std::size_t state_count_{};
    std::vector<HydroAction> actions_;
};

} // namespace optiflow
