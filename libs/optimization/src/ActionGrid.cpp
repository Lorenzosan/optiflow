#include "optiflow/numerics/ActionGrid.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <utility>

namespace optiflow::numerics {

ActionGrid::ActionGrid(std::vector<core::Action> actions) : actions_(std::move(actions)) {
    if (actions_.empty()) {
        throw std::invalid_argument("action grid must contain at least one action");
    }
}

ActionGrid ActionGrid::from_parameters(const core::ModelParameters& model_parameters,
                                       const core::SolverParameters& solver_parameters) {
    const std::vector<double> turbine_axis = uniform_axis(model_parameters.turbine_max_flow,
                                                          solver_parameters.turbine_flow_steps,
                                                          "turbine_flow_steps");
    const std::vector<double> spill_axis = uniform_axis(model_parameters.spill_max_flow,
                                                        solver_parameters.spill_flow_steps,
                                                        "spill_flow_steps");
    const std::vector<double> pump_axis = uniform_axis(model_parameters.pump_max_flow,
                                                       solver_parameters.pump_flow_steps,
                                                       "pump_flow_steps");

    std::vector<core::Action> actions;
    actions.reserve(turbine_axis.size() * spill_axis.size() * pump_axis.size());
    for (double turbine_flow : turbine_axis) {
        for (double spill_flow : spill_axis) {
            for (double pump_flow : pump_axis) {
                if ((turbine_flow > 0.0 && pump_flow > 0.0) ||
                    (spill_flow > 0.0 && pump_flow > 0.0)) {
                    continue;
                }
                actions.emplace_back(turbine_flow, spill_flow, pump_flow);
            }
        }
    }
    return ActionGrid(std::move(actions));
}

const std::vector<core::Action>& ActionGrid::actions() const { return actions_; }
std::size_t ActionGrid::size() const { return actions_.size(); }

std::vector<double> ActionGrid::uniform_axis(double max_value,
                                             std::size_t steps,
                                             const char* name) {
    if (!std::isfinite(max_value)) {
        throw std::invalid_argument(std::string(name) + " max value must be finite");
    }
    if (max_value < 0.0) {
        throw std::invalid_argument(std::string(name) + " max value must be nonnegative");
    }
    if (steps == 0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }

    std::vector<double> axis;
    axis.reserve(steps);
    if (steps == 1 || max_value == 0.0) {
        axis.push_back(0.0);
        return axis;
    }

    const double step = max_value / static_cast<double>(steps - 1);
    for (std::size_t index = 0; index < steps; ++index) {
        axis.push_back(step * static_cast<double>(index));
    }
    return axis;
}

}  // namespace optiflow::numerics
