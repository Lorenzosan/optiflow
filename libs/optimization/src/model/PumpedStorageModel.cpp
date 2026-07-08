#include "optiflow/model/PumpedStorageModel.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace optiflow {
namespace {

constexpr double water_density_kg_m3 = 1000.0;
constexpr double gravity_m_s2 = 9.81;
constexpr double watts_per_megawatt = 1.0e6;
constexpr double seconds_per_hour = 3600.0;
constexpr double tolerance = 1.0e-9;

[[nodiscard]] auto non_negative(const double value) noexcept -> bool {
  return value >= -tolerance;
}

[[nodiscard]] auto within(const double value, const double lower, const double upper) noexcept -> bool {
  return value >= lower - tolerance && value <= upper + tolerance;
}

}  // namespace

PumpedStorageModel::PumpedStorageModel(ModelParameters parameters)
    : m_parameters(std::move(parameters)) {
  if (m_parameters.timestep_hours <= 0.0) {
    throw std::invalid_argument{"timestep_hours must be positive"};
  }

  if (m_parameters.hydro.max_reservoir_volume_m3 < m_parameters.hydro.min_reservoir_volume_m3) {
    throw std::invalid_argument{"reservoir maximum must be greater than or equal to minimum"};
  }

  if (m_parameters.hydro.turbine_efficiency <= 0.0 || m_parameters.hydro.turbine_efficiency > 1.0) {
    throw std::invalid_argument{"turbine efficiency must be in (0, 1]"};
  }

  if (m_parameters.hydro.pump_efficiency <= 0.0 || m_parameters.hydro.pump_efficiency > 1.0) {
    throw std::invalid_argument{"pump efficiency must be in (0, 1]"};
  }

  if (m_parameters.battery.enabled) {
    if (m_parameters.battery.capacity_mwh < 0.0
        || m_parameters.battery.charge_efficiency <= 0.0
        || m_parameters.battery.charge_efficiency > 1.0
        || m_parameters.battery.discharge_efficiency <= 0.0
        || m_parameters.battery.discharge_efficiency > 1.0) {
      throw std::invalid_argument{"invalid battery parameters"};
    }
  }
}

auto PumpedStorageModel::parameters() const noexcept -> const ModelParameters& {
  return m_parameters;
}

auto PumpedStorageModel::evaluate(const State state,
                                  const Action action,
                                  const Exogenous exogenous) const -> Outcome {
  Outcome outcome{};

  if (!is_state_feasible(state) || !is_action_structurally_feasible(action)) {
    outcome.feasible = false;
    return outcome;
  }

  const auto seconds = timestep_seconds();
  const auto hours = m_parameters.timestep_hours;

  outcome.turbine_power_mw = turbine_power_mw(action.turbine_flow_m3_s);
  outcome.pump_power_mw = pump_power_mw(action.pump_flow_m3_s);
  outcome.battery_power_mw = action.battery_discharge_mw - action.battery_charge_mw;
  outcome.net_power_mw = outcome.turbine_power_mw
                       + action.battery_discharge_mw
                       - outcome.pump_power_mw
                       - action.battery_charge_mw;

  const auto reservoir_delta_m3 = (exogenous.natural_inflow_m3_s
                                + action.pump_flow_m3_s
                                - action.turbine_flow_m3_s
                                - action.spill_flow_m3_s) * seconds;

  const auto battery_delta_mwh = action.battery_charge_mw * m_parameters.battery.charge_efficiency * hours
                               - action.battery_discharge_mw / m_parameters.battery.discharge_efficiency * hours;

  outcome.next_state = State{
      .reservoir_volume_m3 = state.reservoir_volume_m3 + reservoir_delta_m3,
      .battery_soc_mwh = state.battery_soc_mwh + battery_delta_mwh,
  };

  if (!is_state_feasible(outcome.next_state)) {
    outcome.feasible = false;
    return outcome;
  }

  outcome.market_revenue_eur = exogenous.price_eur_per_mwh * outcome.net_power_mw * hours;

  const auto turbine_mwh = outcome.turbine_power_mw * hours;
  const auto pump_mwh = outcome.pump_power_mw * hours;
  const auto battery_throughput_mwh = (action.battery_charge_mw + action.battery_discharge_mw) * hours;

  outcome.operating_cost_eur = turbine_mwh * m_parameters.hydro.turbine_cost_eur_per_mwh
                             + pump_mwh * m_parameters.hydro.pump_cost_eur_per_mwh
                             + battery_throughput_mwh * m_parameters.battery.degradation_cost_eur_per_mwh;

  outcome.penalty_cost_eur = action.spill_flow_m3_s * seconds * m_parameters.hydro.spill_penalty_eur_per_m3;
  outcome.reward_eur = outcome.market_revenue_eur - outcome.operating_cost_eur - outcome.penalty_cost_eur;
  outcome.feasible = true;

  return outcome;
}

auto PumpedStorageModel::terminal_value(const State state) const -> double {
  if (!is_state_feasible(state)) {
    return 0.0;
  }

  return state.reservoir_volume_m3 * m_parameters.terminal_water_value_eur_per_m3
       + state.battery_soc_mwh * m_parameters.terminal_battery_value_eur_per_mwh;
}

auto PumpedStorageModel::is_state_feasible(const State state) const noexcept -> bool {
  const auto battery_max = m_parameters.battery.enabled ? m_parameters.battery.capacity_mwh : 0.0;

  return within(state.reservoir_volume_m3,
                m_parameters.hydro.min_reservoir_volume_m3,
                m_parameters.hydro.max_reservoir_volume_m3)
      && within(state.battery_soc_mwh, 0.0, battery_max);
}

auto PumpedStorageModel::is_action_structurally_feasible(const Action action) const noexcept -> bool {
  const auto hydro_ok = non_negative(action.turbine_flow_m3_s)
                     && non_negative(action.spill_flow_m3_s)
                     && non_negative(action.pump_flow_m3_s)
                     && action.turbine_flow_m3_s <= m_parameters.hydro.max_turbine_flow_m3_s + tolerance
                     && action.spill_flow_m3_s <= m_parameters.hydro.max_spill_flow_m3_s + tolerance
                     && action.pump_flow_m3_s <= m_parameters.hydro.max_pump_flow_m3_s + tolerance;

  if (!hydro_ok) {
    return false;
  }

  if (!m_parameters.battery.enabled) {
    return std::abs(action.battery_charge_mw) <= tolerance
        && std::abs(action.battery_discharge_mw) <= tolerance;
  }

  return non_negative(action.battery_charge_mw)
      && non_negative(action.battery_discharge_mw)
      && action.battery_charge_mw <= m_parameters.battery.max_charge_mw + tolerance
      && action.battery_discharge_mw <= m_parameters.battery.max_discharge_mw + tolerance;
}

auto PumpedStorageModel::turbine_power_mw(const double flow_m3_s) const noexcept -> double {
  return water_density_kg_m3
       * gravity_m_s2
       * m_parameters.hydro.hydraulic_head_m
       * flow_m3_s
       * m_parameters.hydro.turbine_efficiency
       / watts_per_megawatt;
}

auto PumpedStorageModel::pump_power_mw(const double flow_m3_s) const noexcept -> double {
  return water_density_kg_m3
       * gravity_m_s2
       * m_parameters.hydro.hydraulic_head_m
       * flow_m3_s
       / (m_parameters.hydro.pump_efficiency * watts_per_megawatt);
}

auto PumpedStorageModel::timestep_seconds() const noexcept -> double {
  return m_parameters.timestep_hours * seconds_per_hour;
}

}  // namespace optiflow
