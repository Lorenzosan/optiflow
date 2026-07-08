#include "optiflow/core/StorageTypes.h"

#include <cmath>
#include <utility>

namespace optiflow::core {

namespace {

void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

}  // namespace

State::State(double reservoir_volume_value, double battery_soc_value)
    : reservoir_volume(reservoir_volume_value), battery_soc(battery_soc_value) {}

Action::Action(double turbine_flow_value,
               double spill_flow_value,
               double pump_flow_value,
               double battery_charge_power_value,
               double battery_discharge_power_value)
    : turbine_flow(turbine_flow_value),
      spill_flow(spill_flow_value),
      pump_flow(pump_flow_value),
      battery_charge_power(battery_charge_power_value),
      battery_discharge_power(battery_discharge_power_value) {}

Exogenous::Exogenous(double electricity_price_value, double natural_inflow_value)
    : electricity_price(electricity_price_value), natural_inflow(natural_inflow_value) {}

Outcome::Outcome(State next_state_value,
                 double turbine_power_value,
                 double pump_power_value,
                 double net_power_value,
                 double reward_value,
                 bool feasible_value,
                 std::string infeasibility_reason_value)
    : next_state(next_state_value),
      turbine_power(turbine_power_value),
      pump_power(pump_power_value),
      net_power(net_power_value),
      reward(reward_value),
      feasible(feasible_value),
      infeasibility_reason(std::move(infeasibility_reason_value)) {}

DispatchStep::DispatchStep(std::size_t time_index_value,
                           State state_value,
                           Action action_value,
                           Exogenous exogenous_value,
                           State next_state_value,
                           double net_power_value,
                           double reward_value,
                           double cumulative_profit_value)
    : time_index(time_index_value),
      state(state_value),
      action(action_value),
      exogenous(exogenous_value),
      next_state(next_state_value),
      net_power(net_power_value),
      reward(reward_value),
      cumulative_profit(cumulative_profit_value) {}

ModelParameters::ModelParameters(double time_step_hours_value,
                                 double reservoir_min_volume_value,
                                 double reservoir_max_volume_value,
                                 double battery_min_soc_value,
                                 double battery_max_soc_value,
                                 double turbine_max_flow_value,
                                 double pump_max_flow_value,
                                 double spill_max_flow_value,
                                 double battery_max_charge_power_value,
                                 double battery_max_discharge_power_value,
                                 double turbine_efficiency_value,
                                 double pump_efficiency_value,
                                 double battery_charge_efficiency_value,
                                 double battery_discharge_efficiency_value,
                                 double water_to_power_factor_value,
                                 double battery_degradation_cost_per_mwh_value,
                                 double operating_cost_per_mwh_value,
                                 double infeasibility_penalty_value)
    : time_step_hours(time_step_hours_value),
      reservoir_min_volume(reservoir_min_volume_value),
      reservoir_max_volume(reservoir_max_volume_value),
      battery_min_soc(battery_min_soc_value),
      battery_max_soc(battery_max_soc_value),
      turbine_max_flow(turbine_max_flow_value),
      pump_max_flow(pump_max_flow_value),
      spill_max_flow(spill_max_flow_value),
      battery_max_charge_power(battery_max_charge_power_value),
      battery_max_discharge_power(battery_max_discharge_power_value),
      turbine_efficiency(turbine_efficiency_value),
      pump_efficiency(pump_efficiency_value),
      battery_charge_efficiency(battery_charge_efficiency_value),
      battery_discharge_efficiency(battery_discharge_efficiency_value),
      water_to_power_factor(water_to_power_factor_value),
      battery_degradation_cost_per_mwh(battery_degradation_cost_per_mwh_value),
      operating_cost_per_mwh(operating_cost_per_mwh_value),
      infeasibility_penalty(infeasibility_penalty_value) {}

TerminalParameters::TerminalParameters(double reservoir_min_volume_value,
                                       double reservoir_max_volume_value,
                                       double battery_min_soc_value,
                                       double battery_max_soc_value,
                                       double target_reservoir_volume_value,
                                       double target_battery_soc_value,
                                       double reservoir_target_penalty_value,
                                       double battery_target_penalty_value)
    : reservoir_min_volume(reservoir_min_volume_value),
      reservoir_max_volume(reservoir_max_volume_value),
      battery_min_soc(battery_min_soc_value),
      battery_max_soc(battery_max_soc_value),
      target_reservoir_volume(target_reservoir_volume_value),
      target_battery_soc(target_battery_soc_value),
      reservoir_target_penalty(reservoir_target_penalty_value),
      battery_target_penalty(battery_target_penalty_value) {}

SolverParameters::SolverParameters(std::size_t reservoir_volume_grid_points_value,
                                   std::size_t battery_soc_grid_points_value,
                                   std::size_t turbine_flow_steps_value,
                                   std::size_t spill_flow_steps_value,
                                   std::size_t pump_flow_steps_value,
                                   std::size_t battery_charge_steps_value,
                                   std::size_t battery_discharge_steps_value,
                                   double discount_factor_value)
    : reservoir_volume_grid_points(reservoir_volume_grid_points_value),
      battery_soc_grid_points(battery_soc_grid_points_value),
      turbine_flow_steps(turbine_flow_steps_value),
      spill_flow_steps(spill_flow_steps_value),
      pump_flow_steps(pump_flow_steps_value),
      battery_charge_steps(battery_charge_steps_value),
      battery_discharge_steps(battery_discharge_steps_value),
      discount_factor(discount_factor_value) {}

void validate_model_parameters(const ModelParameters& parameters) {
    require_finite(parameters.time_step_hours, "time_step_hours");
    require_finite(parameters.reservoir_min_volume, "reservoir_min_volume");
    require_finite(parameters.reservoir_max_volume, "reservoir_max_volume");
    require_finite(parameters.battery_min_soc, "battery_min_soc");
    require_finite(parameters.battery_max_soc, "battery_max_soc");
    require_finite(parameters.turbine_max_flow, "turbine_max_flow");
    require_finite(parameters.pump_max_flow, "pump_max_flow");
    require_finite(parameters.spill_max_flow, "spill_max_flow");
    require_finite(parameters.battery_max_charge_power, "battery_max_charge_power");
    require_finite(parameters.battery_max_discharge_power, "battery_max_discharge_power");
    require_finite(parameters.turbine_efficiency, "turbine_efficiency");
    require_finite(parameters.pump_efficiency, "pump_efficiency");
    require_finite(parameters.battery_charge_efficiency, "battery_charge_efficiency");
    require_finite(parameters.battery_discharge_efficiency, "battery_discharge_efficiency");
    require_finite(parameters.water_to_power_factor, "water_to_power_factor");
    require_finite(parameters.battery_degradation_cost_per_mwh, "battery_degradation_cost_per_mwh");
    require_finite(parameters.operating_cost_per_mwh, "operating_cost_per_mwh");
    require_finite(parameters.infeasibility_penalty, "infeasibility_penalty");

    if (parameters.time_step_hours <= 0.0) {
        throw std::invalid_argument("time_step_hours must be positive");
    }
    if (parameters.reservoir_min_volume > parameters.reservoir_max_volume) {
        throw std::invalid_argument("reservoir_min_volume exceeds reservoir_max_volume");
    }
    if (parameters.battery_min_soc > parameters.battery_max_soc) {
        throw std::invalid_argument("battery_min_soc exceeds battery_max_soc");
    }
    if (parameters.turbine_max_flow < 0.0 || parameters.pump_max_flow < 0.0 ||
        parameters.spill_max_flow < 0.0 || parameters.battery_max_charge_power < 0.0 ||
        parameters.battery_max_discharge_power < 0.0) {
        throw std::invalid_argument("action limits must be nonnegative");
    }
    if (parameters.turbine_efficiency <= 0.0 || parameters.pump_efficiency <= 0.0 ||
        parameters.battery_charge_efficiency <= 0.0 ||
        parameters.battery_discharge_efficiency <= 0.0) {
        throw std::invalid_argument("efficiencies must be positive");
    }
    if (parameters.water_to_power_factor <= 0.0) {
        throw std::invalid_argument("water_to_power_factor must be positive");
    }
    if (parameters.battery_degradation_cost_per_mwh < 0.0 ||
        parameters.operating_cost_per_mwh < 0.0 || parameters.infeasibility_penalty < 0.0) {
        throw std::invalid_argument("costs and penalties must be nonnegative");
    }
}

void validate_terminal_parameters(const ModelParameters& model_parameters,
                                  const TerminalParameters& terminal_parameters) {
    require_finite(terminal_parameters.reservoir_min_volume, "terminal_reservoir_min_volume");
    require_finite(terminal_parameters.reservoir_max_volume, "terminal_reservoir_max_volume");
    require_finite(terminal_parameters.battery_min_soc, "terminal_battery_min_soc");
    require_finite(terminal_parameters.battery_max_soc, "terminal_battery_max_soc");
    require_finite(terminal_parameters.target_reservoir_volume, "terminal_target_reservoir_volume");
    require_finite(terminal_parameters.target_battery_soc, "terminal_target_battery_soc");
    require_finite(terminal_parameters.reservoir_target_penalty, "terminal_reservoir_target_penalty");
    require_finite(terminal_parameters.battery_target_penalty, "terminal_battery_target_penalty");

    if (terminal_parameters.reservoir_min_volume > terminal_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal_reservoir_min_volume exceeds terminal_reservoir_max_volume");
    }
    if (terminal_parameters.battery_min_soc > terminal_parameters.battery_max_soc) {
        throw std::invalid_argument("terminal_battery_min_soc exceeds terminal_battery_max_soc");
    }

    if (terminal_parameters.reservoir_min_volume < model_parameters.reservoir_min_volume ||
        terminal_parameters.reservoir_max_volume > model_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal reservoir bounds must be inside model reservoir bounds");
    }
    if (terminal_parameters.battery_min_soc < model_parameters.battery_min_soc ||
        terminal_parameters.battery_max_soc > model_parameters.battery_max_soc) {
        throw std::invalid_argument("terminal battery bounds must be inside model battery bounds");
    }

    if (terminal_parameters.target_reservoir_volume < terminal_parameters.reservoir_min_volume ||
        terminal_parameters.target_reservoir_volume > terminal_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal_target_reservoir_volume must be inside terminal reservoir bounds");
    }
    if (terminal_parameters.target_battery_soc < terminal_parameters.battery_min_soc ||
        terminal_parameters.target_battery_soc > terminal_parameters.battery_max_soc) {
        throw std::invalid_argument("terminal_target_battery_soc must be inside terminal battery bounds");
    }

    if (terminal_parameters.reservoir_target_penalty < 0.0 ||
        terminal_parameters.battery_target_penalty < 0.0) {
        throw std::invalid_argument("terminal target penalties must be nonnegative");
    }
}

void validate_solver_parameters(const SolverParameters& parameters) {
    if (parameters.reservoir_volume_grid_points == 0 || parameters.battery_soc_grid_points == 0 ||
        parameters.turbine_flow_steps == 0 || parameters.spill_flow_steps == 0 ||
        parameters.pump_flow_steps == 0 || parameters.battery_charge_steps == 0 ||
        parameters.battery_discharge_steps == 0) {
        throw std::invalid_argument("grid point and action step counts must be positive");
    }
    require_finite(parameters.discount_factor, "discount_factor");
    if (parameters.discount_factor < 0.0 || parameters.discount_factor > 1.0) {
        throw std::invalid_argument("discount_factor must be in [0, 1]");
    }
}

}  // namespace optiflow::core
