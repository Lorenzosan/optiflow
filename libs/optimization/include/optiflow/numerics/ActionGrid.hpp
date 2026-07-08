#pragma once

#include <cstddef>
#include <vector>

#include <optiflow/core/StorageTypes.hpp>

namespace optiflow {

/**
 * @brief Configuration for discretizing deterministic hydro actions.
 */
struct ActionGridConfig {
    double max_turbine_flow_m3_s{};
    double max_pump_flow_m3_s{};
    std::size_t turbine_flow_steps{};
    std::size_t pump_flow_steps{};
};

/**
 * @brief Finite set of physically meaningful hydro actions.
 *
 * The grid contains idle, turbine-only actions, and pump-only actions. It does
 * not contain simultaneous pumping and turbining.
 */
class ActionGrid {
public:
    /**
     * @brief Construct an action grid from a discretization configuration.
     *
     * @throws std::invalid_argument if the configuration is inconsistent.
     */
    explicit ActionGrid(const ActionGridConfig& config);

    /**
     * @brief Return all candidate actions.
     */
    [[nodiscard]] const std::vector<HydroAction>& actions() const noexcept;

    /**
     * @brief Return the number of candidate actions.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return action at index.
     *
     * @throws std::out_of_range if index is outside the action grid.
     */
    [[nodiscard]] const HydroAction& at(std::size_t index) const;

private:
    std::vector<HydroAction> actions_;
};

} // namespace optiflow
