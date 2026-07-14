#include "optiflow/core/StorageTypes.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace optiflow::core {

namespace {

void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

void require_nonnegative(double value, const char* name) {
    require_finite(value, name);
    if (value < 0.0) {
        throw std::invalid_argument(std::string(name) + " must be nonnegative");
    }
}

void require_positive(double value, const char* name) {
    require_finite(value, name);
    if (value <= 0.0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

void require_positive(std::size_t value, const char* name) {
    if (value == 0) {
        throw std::invalid_argument(std::string(name) + " must be positive");
    }
}

}  // namespace

State::State(double reservoir_volume_value) : reservoir_volume(reservoir_volume_value) {}

Action::Action(double turbine_flow_value, double spill_flow_value, double pump_flow_value)
    : turbine_flow(turbine_flow_value),
      spill_flow(spill_flow_value),
      pump_flow(pump_flow_value) {}

Exogenous::Exogenous(std::string timestamp_utc_value,
                       double electricity_price_value,
                       double natural_inflow_value)
    : timestamp_utc(std::move(timestamp_utc_value)),
      electricity_price(electricity_price_value),
      natural_inflow(natural_inflow_value) {}

Exogenous::Exogenous(double electricity_price_value, double natural_inflow_value)
    : Exogenous("", electricity_price_value, natural_inflow_value) {}

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
                                 double turbine_max_flow_value,
                                 double pump_max_flow_value,
                                 double spill_max_flow_value,
                                 double turbine_efficiency_value,
                                 double pump_efficiency_value,
                                 double operating_cost_per_mwh_value)
    : time_step_hours(time_step_hours_value),
      reservoir_min_volume(reservoir_min_volume_value),
      reservoir_max_volume(reservoir_max_volume_value),
      turbine_max_flow(turbine_max_flow_value),
      pump_max_flow(pump_max_flow_value),
      spill_max_flow(spill_max_flow_value),
      turbine_efficiency(turbine_efficiency_value),
      pump_efficiency(pump_efficiency_value),
      operating_cost_per_mwh(operating_cost_per_mwh_value) {}

TerminalParameters::TerminalParameters(double reservoir_min_volume_value,
                                       double reservoir_max_volume_value,
                                       double target_reservoir_volume_value,
                                       double reservoir_target_penalty_value)
    : reservoir_min_volume(reservoir_min_volume_value),
      reservoir_max_volume(reservoir_max_volume_value),
      target_reservoir_volume(target_reservoir_volume_value),
      reservoir_target_penalty(reservoir_target_penalty_value) {}

SolverParameters::SolverParameters(std::size_t reservoir_volume_grid_points_value,
                                   std::size_t turbine_flow_steps_value,
                                   std::size_t spill_flow_steps_value,
                                   std::size_t pump_flow_steps_value,
                                   double discount_factor_value)
    : reservoir_volume_grid_points(reservoir_volume_grid_points_value),
      turbine_flow_steps(turbine_flow_steps_value),
      spill_flow_steps(spill_flow_steps_value),
      pump_flow_steps(pump_flow_steps_value),
      discount_factor(discount_factor_value) {}

void validate_model_parameters(const ModelParameters& parameters) {
    require_positive(parameters.time_step_hours, "time_step_hours");
    require_finite(parameters.reservoir_min_volume, "reservoir_min_volume");
    require_finite(parameters.reservoir_max_volume, "reservoir_max_volume");
    if (parameters.reservoir_min_volume > parameters.reservoir_max_volume) {
        throw std::invalid_argument("reservoir minimum exceeds maximum");
    }

    require_nonnegative(parameters.turbine_max_flow, "turbine_max_flow");
    require_nonnegative(parameters.pump_max_flow, "pump_max_flow");
    require_nonnegative(parameters.spill_max_flow, "spill_max_flow");

    require_positive(parameters.turbine_efficiency, "turbine_efficiency");
    require_positive(parameters.pump_efficiency, "pump_efficiency");
    if (parameters.turbine_efficiency > 1.0 || parameters.pump_efficiency > 1.0) {
        throw std::invalid_argument("efficiencies must not exceed one");
    }

    require_nonnegative(parameters.operating_cost_per_mwh, "operating_cost_per_mwh");
}

void validate_terminal_parameters(const ModelParameters& model_parameters,
                                  const TerminalParameters& terminal_parameters) {
    require_finite(terminal_parameters.reservoir_min_volume, "terminal_reservoir_min_volume");
    require_finite(terminal_parameters.reservoir_max_volume, "terminal_reservoir_max_volume");
    require_finite(terminal_parameters.target_reservoir_volume, "terminal_target_reservoir_volume");
    require_nonnegative(terminal_parameters.reservoir_target_penalty,
                        "terminal_reservoir_target_penalty");

    if (terminal_parameters.reservoir_min_volume > terminal_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal reservoir minimum exceeds maximum");
    }
    if (terminal_parameters.reservoir_min_volume < model_parameters.reservoir_min_volume ||
        terminal_parameters.reservoir_max_volume > model_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal reservoir bounds must be inside model bounds");
    }
    if (terminal_parameters.target_reservoir_volume < terminal_parameters.reservoir_min_volume ||
        terminal_parameters.target_reservoir_volume > terminal_parameters.reservoir_max_volume) {
        throw std::invalid_argument("terminal reservoir target must be inside terminal bounds");
    }
}

void validate_solver_parameters(const SolverParameters& parameters) {
    require_positive(parameters.reservoir_volume_grid_points, "reservoir_volume_grid_points");
    require_positive(parameters.turbine_flow_steps, "turbine_flow_steps");
    require_positive(parameters.spill_flow_steps, "spill_flow_steps");
    require_positive(parameters.pump_flow_steps, "pump_flow_steps");
    require_finite(parameters.discount_factor, "discount_factor");
    if (parameters.discount_factor < 0.0 || parameters.discount_factor > 1.0) {
        throw std::invalid_argument("discount_factor must be between zero and one");
    }
}

}  // namespace optiflow::core
