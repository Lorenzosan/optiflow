#include <optiflow/numerics/ActionGrid.hpp>

#include <stdexcept>

namespace optiflow {

ActionGrid::ActionGrid(const ActionGridConfig& config) {
    if (config.max_turbine_flow_m3_s < 0.0 || config.max_pump_flow_m3_s < 0.0) {
        throw std::invalid_argument("maximum hydro flows must be non-negative");
    }

    actions_.push_back(HydroAction{HydroMode::Idle, 0.0, 0.0});

    if (config.max_turbine_flow_m3_s > 0.0) {
        if (config.turbine_flow_steps == 0) {
            throw std::invalid_argument("turbine flow steps must be positive when turbine flow is enabled");
        }
        const double step = config.max_turbine_flow_m3_s /
                            static_cast<double>(config.turbine_flow_steps);
        for (std::size_t index = 1; index <= config.turbine_flow_steps; ++index) {
            actions_.push_back(HydroAction{HydroMode::Turbine,
                                           static_cast<double>(index) * step,
                                           0.0});
        }
    }

    if (config.max_pump_flow_m3_s > 0.0) {
        if (config.pump_flow_steps == 0) {
            throw std::invalid_argument("pump flow steps must be positive when pump flow is enabled");
        }
        const double step = config.max_pump_flow_m3_s / static_cast<double>(config.pump_flow_steps);
        for (std::size_t index = 1; index <= config.pump_flow_steps; ++index) {
            actions_.push_back(HydroAction{HydroMode::Pump, 0.0, static_cast<double>(index) * step});
        }
    }
}

const std::vector<HydroAction>& ActionGrid::actions() const noexcept { return actions_; }

std::size_t ActionGrid::size() const noexcept { return actions_.size(); }

const HydroAction& ActionGrid::at(std::size_t index) const { return actions_.at(index); }

} // namespace optiflow
