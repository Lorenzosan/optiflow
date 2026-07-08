#include <optiflow/model/PumpedStorageModel.hpp>

#include <algorithm>
#include <stdexcept>

namespace optiflow {

PumpedStorageModel::PumpedStorageModel(ModelParameters parameters) : parameters_(parameters) {
    validate_parameters(parameters_);
}

const ModelParameters& PumpedStorageModel::parameters() const noexcept { return parameters_; }

TransitionResult PumpedStorageModel::transition(const ReservoirState& state,
                                                const HydroAction& action,
                                                const ExogenousPoint& exogenous) const {
    if (state.reservoir_volume_m3 < parameters_.min_reservoir_volume_m3 ||
        state.reservoir_volume_m3 > parameters_.max_reservoir_volume_m3) {
        return TransitionResult{};
    }

    if (action.turbine_flow_m3_s < 0.0 || action.pump_flow_m3_s < 0.0 ||
        action.turbine_flow_m3_s > parameters_.max_turbine_flow_m3_s ||
        action.pump_flow_m3_s > parameters_.max_pump_flow_m3_s ||
        (action.turbine_flow_m3_s > 0.0 && action.pump_flow_m3_s > 0.0)) {
        return TransitionResult{};
    }

    const double timestep_seconds = parameters_.timestep_hours * 3600.0;
    const double raw_next_volume = state.reservoir_volume_m3 +
                                   timestep_seconds * exogenous.natural_inflow_m3_s -
                                   timestep_seconds * action.turbine_flow_m3_s +
                                   timestep_seconds * action.pump_flow_m3_s;

    if (raw_next_volume < parameters_.min_reservoir_volume_m3) {
        return TransitionResult{};
    }

    const double overflow_spill_m3 =
        std::max(0.0, raw_next_volume - parameters_.max_reservoir_volume_m3);
    const double next_volume = raw_next_volume - overflow_spill_m3;

    const double turbine_power = turbine_power_mw(action.turbine_flow_m3_s,
                                                  parameters_.hydraulic_head_m,
                                                  parameters_.turbine_efficiency);
    const double pump_power = pump_power_mw(action.pump_flow_m3_s,
                                            parameters_.hydraulic_head_m,
                                            parameters_.pump_efficiency);
    const double net_power = turbine_power - pump_power;
    const double market_revenue = exogenous.price_eur_per_mwh * net_power * parameters_.timestep_hours;
    const double spill_penalty = parameters_.overflow_spill_penalty_eur_per_m3 * overflow_spill_m3;

    return TransitionResult{true,
                            next_volume,
                            overflow_spill_m3,
                            turbine_power,
                            pump_power,
                            net_power,
                            market_revenue,
                            spill_penalty,
                            market_revenue - spill_penalty};
}

void PumpedStorageModel::validate_parameters(const ModelParameters& parameters) {
    if (parameters.min_reservoir_volume_m3 < 0.0) {
        throw std::invalid_argument("min reservoir volume must be non-negative");
    }
    if (parameters.max_reservoir_volume_m3 <= parameters.min_reservoir_volume_m3) {
        throw std::invalid_argument("max reservoir volume must exceed min reservoir volume");
    }
    if (parameters.initial_reservoir_volume_m3 < parameters.min_reservoir_volume_m3 ||
        parameters.initial_reservoir_volume_m3 > parameters.max_reservoir_volume_m3) {
        throw std::invalid_argument("initial reservoir volume is outside reservoir bounds");
    }
    if (parameters.max_turbine_flow_m3_s < 0.0) {
        throw std::invalid_argument("max turbine flow must be non-negative");
    }
    if (parameters.max_pump_flow_m3_s < 0.0) {
        throw std::invalid_argument("max pump flow must be non-negative");
    }
    if (parameters.hydraulic_head_m <= 0.0) {
        throw std::invalid_argument("hydraulic head must be positive");
    }
    if (parameters.turbine_efficiency <= 0.0 || parameters.turbine_efficiency > 1.0) {
        throw std::invalid_argument("turbine efficiency must be in (0, 1]");
    }
    if (parameters.pump_efficiency <= 0.0 || parameters.pump_efficiency > 1.0) {
        throw std::invalid_argument("pump efficiency must be in (0, 1]");
    }
    if (parameters.timestep_hours <= 0.0) {
        throw std::invalid_argument("timestep hours must be positive");
    }
    if (parameters.discount_factor <= 0.0 || parameters.discount_factor > 1.0) {
        throw std::invalid_argument("discount factor must be in (0, 1]");
    }
    if (parameters.terminal_water_value_eur_per_m3 < 0.0) {
        throw std::invalid_argument("terminal water value must be non-negative");
    }
    if (parameters.overflow_spill_penalty_eur_per_m3 < 0.0) {
        throw std::invalid_argument("overflow spill penalty must be non-negative");
    }
}

double PumpedStorageModel::turbine_power_mw(double turbine_flow_m3_s, double hydraulic_head_m,
                                            double turbine_efficiency) {
    constexpr double water_density_kg_m3 = 1000.0;
    constexpr double gravity_m_s2 = 9.81;
    return water_density_kg_m3 * gravity_m_s2 * hydraulic_head_m * turbine_efficiency *
           turbine_flow_m3_s / 1.0e6;
}

double PumpedStorageModel::pump_power_mw(double pump_flow_m3_s, double hydraulic_head_m,
                                         double pump_efficiency) {
    constexpr double water_density_kg_m3 = 1000.0;
    constexpr double gravity_m_s2 = 9.81;
    return water_density_kg_m3 * gravity_m_s2 * hydraulic_head_m * pump_flow_m3_s /
           (pump_efficiency * 1.0e6);
}

} // namespace optiflow
