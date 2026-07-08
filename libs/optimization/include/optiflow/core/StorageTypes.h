#pragma once

#include <cstddef>
#include <limits>
#include <vector>

namespace optiflow {

/** Physical storage state used by the dynamic program. */
struct State final {
  double reservoir_volume_m3{};
  double battery_soc_mwh{};

  [[nodiscard]] friend constexpr auto operator==(const State&, const State&) -> bool = default;
};

/** Dispatch decision for one time step. */
struct Action final {
  double turbine_flow_m3_s{};
  double spill_flow_m3_s{};
  double pump_flow_m3_s{};
  double battery_charge_mw{};
  double battery_discharge_mw{};

  [[nodiscard]] friend constexpr auto operator==(const Action&, const Action&) -> bool = default;
};

/** Exogenous input known at one time step. */
struct Exogenous final {
  double price_eur_per_mwh{};
  double natural_inflow_m3_s{};
};

/** Result of applying an action to a state under one exogenous input. */
struct Outcome final {
  State next_state{};
  double turbine_power_mw{};
  double pump_power_mw{};
  double battery_power_mw{};
  double net_power_mw{};
  double market_revenue_eur{};
  double operating_cost_eur{};
  double penalty_cost_eur{};
  double reward_eur{};
  bool feasible{};
};

/** One row of the forward-simulated dispatch trajectory. */
struct DispatchStep final {
  std::size_t time_index{};
  State state{};
  Action action{};
  Exogenous exogenous{};
  Outcome outcome{};
  double cumulative_profit_eur{};
};

/** Hydro plant parameters used by the simplified pumped-storage model. */
struct HydroParameters final {
  double min_reservoir_volume_m3{};
  double max_reservoir_volume_m3{};
  double max_turbine_flow_m3_s{};
  double max_pump_flow_m3_s{};
  double max_spill_flow_m3_s{};
  double hydraulic_head_m{};
  double turbine_efficiency{};
  double pump_efficiency{};
  double turbine_cost_eur_per_mwh{};
  double pump_cost_eur_per_mwh{};
  double spill_penalty_eur_per_m3{};
};

/** Optional battery parameters. Set enabled to false for hydro-only mode. */
struct BatteryParameters final {
  bool enabled{};
  double capacity_mwh{};
  double max_charge_mw{};
  double max_discharge_mw{};
  double charge_efficiency{};
  double discharge_efficiency{};
  double degradation_cost_eur_per_mwh{};
};

/** Full physical and economic model configuration. */
struct ModelParameters final {
  HydroParameters hydro{};
  BatteryParameters battery{};
  double timestep_hours{};
  double terminal_water_value_eur_per_m3{};
  double terminal_battery_value_eur_per_mwh{};
};

/** Scenario input to the optimizer. */
struct Scenario final {
  ModelParameters parameters{};
  std::vector<Exogenous> exogenous{};
};

/** Numerical and policy constraints for Bellman optimization. */
struct OptimizationConfig final {
  double discount_factor{1.0};
  double infeasible_value{-std::numeric_limits<double>::infinity()};
  bool forbid_simultaneous_pump_and_turbine{true};
  bool forbid_simultaneous_charge_and_discharge{true};
};

}  // namespace optiflow
