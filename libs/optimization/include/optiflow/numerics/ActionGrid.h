#pragma once

#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <vector>

namespace optiflow::numerics {

class ActionGrid {
public:
    explicit ActionGrid(std::vector<core::Action> actions);

    static ActionGrid from_parameters(const core::ModelParameters& model_parameters,
                                      const core::SolverParameters& solver_parameters);

    const std::vector<core::Action>& actions() const;
    std::size_t size() const;

private:
    std::vector<core::Action> actions_;

    static std::vector<double> uniform_axis(double max_value,
                                            std::size_t steps,
                                            const char* name);
};

}  // namespace optiflow::numerics
