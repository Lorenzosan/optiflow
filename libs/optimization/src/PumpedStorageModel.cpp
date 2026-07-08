#include "optiflow/model/PumpedStorageModel.h"

#include <cmath>
#include <string>

namespace optiflow::model {

namespace {

constexpr double tolerance = 1.0e-9;

bool is_negative(double value) {
    return value < -tolerance;
}

bool exceeds(double value, double limit) {
    return value > limit + tolerance;
}

}  // namespace

PumpedStorageModel::PumpedStorageModel(core::ModelParameters parameters) : parameters_(parameters) {
    core::validate_model_parameters(parameters_);
}

core::Outcome PumpedStorageModel::apply(core::State state,
                                        core::Action action,
                                        core::Exogenous exogenous) const {
    if (is_negative(action.turbine_flow) || is_negative(action.spill_flow) ||
        is_negative(action.pump_flow) || is_negative(action.battery_charge_power) ||
        is_negative(action.battery_discharge_power)) {
        return infeasible(state, "action contains a negative component");
    }

    if (exceeds(action.turbine_flow, parameters_.turbine_max_flow)) {
        return infeasible(state, "turbine flow exceeds its limit");
    }
    if (exceeds(action.pump_flow, parameters_.pump_max_flow)) {
        return infeasible(state, "pump flow exceeds its limit");
    }
    if (exceeds(action.spill_flow, parameters_.spill_max_flow)) {
        return infeasible(state, "spill flow exceeds its limit");
    }
    if (exceeds(action.battery_charge_power, parameters_.battery_max_charge_power)) {
        return infeasible(state, "battery charge power exceeds its limit");
    }
    if (exceeds(action.battery_discharge_power, parameters_.battery_max_discharge_power)) {
        return infeasible(state, "battery discharge power exceeds its limit");
    }

    if (action.turbine_flow > tolerance && action.pump_flow > tolerance) {
        return infeasible(state, "cannot turbine and pump at the same time");
    }
    if (action.battery_charge_power > tolerance && action.battery_discharge_power > tolerance) {
        return infeasible(state, "cannot charge and discharge the battery at the same time");
    }

    const double dt = parameters_.time_step_hours;
    const double turbine_power = action.turbine_flow * parameters_.water_to_power_factor *
                                 parameters_.turbine_efficiency;
    const double pump_power = action.pump_flow * parameters_.water_to_power_factor /
                              parameters_.pump_efficiency;

    const double next_reservoir_volume = state.reservoir_volume + exogenous.natural_inflow * dt +
                                         action.pump_flow * dt - action.turbine_flow * dt -
                                         action.spill_flow * dt;

    const double next_battery_soc = state.battery_soc +
                                    action.battery_charge_power * parameters_.battery_charge_efficiency * dt -
                                    action.battery_discharge_power / parameters_.battery_discharge_efficiency * dt;

    const core::State next_state(next_reservoir_volume, next_battery_soc);

    if (next_reservoir_volume < parameters_.reservoir_min_volume - tolerance ||
        next_reservoir_volume > parameters_.reservoir_max_volume + tolerance) {
        return infeasible(next_state, "next reservoir volume is outside limits");
    }

    if (next_battery_soc < parameters_.battery_min_soc - tolerance ||
        next_battery_soc > parameters_.battery_max_soc + tolerance) {
        return infeasible(next_state, "next battery state of charge is outside limits");
    }

    const double net_power = turbine_power + action.battery_discharge_power - pump_power -
                             action.battery_charge_power;
    const double revenue = exogenous.electricity_price * net_power * dt;
    const double operating_throughput = (turbine_power + pump_power + action.battery_charge_power +
                                         action.battery_discharge_power) * dt;
    const double operating_cost = parameters_.operating_cost_per_mwh * operating_throughput;
    const double battery_throughput = (action.battery_charge_power + action.battery_discharge_power) * dt;
    const double degradation_cost = parameters_.battery_degradation_cost_per_mwh * battery_throughput;
    const double reward = revenue - operating_cost - degradation_cost;

    return core::Outcome(next_state, turbine_power, pump_power, net_power, reward, true, "");
}

const core::ModelParameters& PumpedStorageModel::parameters() const {
    return parameters_;
}

core::Outcome PumpedStorageModel::infeasible(core::State state, const char* reason) const {
    return core::Outcome(state,
                         0.0,
                         0.0,
                         0.0,
                         -parameters_.infeasibility_penalty,
                         false,
                         std::string(reason));
}

}  // namespace optiflow::model
