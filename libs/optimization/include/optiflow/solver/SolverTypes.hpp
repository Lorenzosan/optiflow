#pragma once

#include <cstddef>

namespace optiflow {

/**
 * @brief Configuration for deterministic Bellman optimization.
 */
struct BellmanSolverConfig {
    std::size_t volume_grid_points{101};
    std::size_t turbine_flow_steps{8};
    std::size_t pump_flow_steps{6};
};

} // namespace optiflow
