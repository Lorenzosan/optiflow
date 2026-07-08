#pragma once

namespace optiflow {

/**
 * @brief Supported hydro operating modes in the deterministic restart.
 */
enum class HydroMode {
    Idle,
    Turbine,
    Pump
};

} // namespace optiflow
