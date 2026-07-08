#pragma once

#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <vector>

namespace optiflow::numerics {

/**
 * @brief Finite action set used by the Bellman solver.
 */
class ActionGrid {
public:
    /**
     * @brief Construct an action grid from explicit action candidates.
     *
     * @param actions Candidate actions.
     */
    explicit ActionGrid(std::vector<core::Action> actions);

    /**
     * @brief Build a uniform action grid from model and solver parameters.
     *
     * @param model_parameters Physical model parameters.
     * @param solver_parameters Numerical solver parameters.
     * @return Action grid.
     */
    static ActionGrid from_parameters(const core::ModelParameters& model_parameters,
                                      const core::SolverParameters& solver_parameters);

    /**
     * @brief Return all candidate actions.
     *
     * @return Candidate action list.
     */
    const std::vector<core::Action>& actions() const;

    /**
     * @brief Return the number of candidate actions.
     *
     * @return Action count.
     */
    std::size_t size() const;

private:
    std::vector<core::Action> actions_;

    /**
     * @brief Build a uniform axis from zero to a maximum value.
     */
    static std::vector<double> uniform_axis(double max_value, std::size_t steps, const char* name);
};

}  // namespace optiflow::numerics
