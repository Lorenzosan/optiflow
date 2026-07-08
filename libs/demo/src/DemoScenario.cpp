#include "optiflow/demo/DemoScenario.h"

#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/solver/BellmanSolver.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace optiflow::demo {
namespace {

constexpr double grid_flow_m3_s = 69.4444444444;

void append_state_json(std::ostringstream& stream, const State& state) {
  stream << "{\"reservoir_volume_m3\":" << state.reservoir_volume_m3
         << ",\"battery_soc_mwh\":" << state.battery_soc_mwh << '}';
}

void append_action_json(std::ostringstream& stream, const Action& action) {
  stream << "{\"turbine_flow_m3_s\":" << action.turbine_flow_m3_s
         << ",\"spill_flow_m3_s\":" << action.spill_flow_m3_s
         << ",\"pump_flow_m3_s\":" << action.pump_flow_m3_s
         << ",\"battery_charge_mw\":" << action.battery_charge_mw
         << ",\"battery_discharge_mw\":" << action.battery_discharge_mw << '}';
}

void append_exogenous_json(std::ostringstream& stream, const Exogenous& exogenous) {
  stream << "{\"price_eur_per_mwh\":" << exogenous.price_eur_per_mwh
         << ",\"natural_inflow_m3_s\":" << exogenous.natural_inflow_m3_s << '}';
}

void append_outcome_json(std::ostringstream& stream, const Outcome& outcome) {
  stream << "{\"next_state\":";
  append_state_json(stream, outcome.next_state);
  stream << ",\"turbine_power_mw\":" << outcome.turbine_power_mw
         << ",\"pump_power_mw\":" << outcome.pump_power_mw
         << ",\"battery_power_mw\":" << outcome.battery_power_mw
         << ",\"net_power_mw\":" << outcome.net_power_mw
         << ",\"market_revenue_eur\":" << outcome.market_revenue_eur
         << ",\"operating_cost_eur\":" << outcome.operating_cost_eur
         << ",\"penalty_cost_eur\":" << outcome.penalty_cost_eur
         << ",\"reward_eur\":" << outcome.reward_eur
         << ",\"feasible\":" << (outcome.feasible ? "true" : "false") << '}';
}



}  // namespace

[[nodiscard]] auto make_default_parameters() -> ModelParameters {
  return ModelParameters{
      .hydro = HydroParameters{
          .min_reservoir_volume_m3 = 0.0,
          .max_reservoir_volume_m3 = 0.0,
          .max_turbine_flow_m3_s = 0.0,
          .max_pump_flow_m3_s = 0.0,
          .max_spill_flow_m3_s = 0.0,
          .hydraulic_head_m = 1.0,
          .turbine_efficiency = 1.0,
          .pump_efficiency = 1.0,
          .turbine_cost_eur_per_mwh = 0.0,
          .pump_cost_eur_per_mwh = 0.0,
          .spill_penalty_eur_per_m3 = 0.0,
      },
      .battery = BatteryParameters{
          .enabled = true,
          .capacity_mwh = 1.0,
          .max_charge_mw = 1.0,
          .max_discharge_mw = 1.0,
          .charge_efficiency = 1.0,
          .discharge_efficiency = 1.0,
          .degradation_cost_eur_per_mwh = 0.0,
      },
      .timestep_hours = 1.0,
      .terminal_water_value_eur_per_m3 = 0.0,
      .terminal_battery_value_eur_per_mwh = 0.0,
  };
}

[[nodiscard]] auto make_default_exogenous() -> std::vector<Exogenous> {
  constexpr auto hours_in_month = 30U * 24U;

  std::vector<Exogenous> exogenous(
      hours_in_month,
      Exogenous{
          .price_eur_per_mwh = 50.0,
          .natural_inflow_m3_s = 0.0,
      });

  exogenous[100].price_eur_per_mwh = 10.0;
  exogenous[500].price_eur_per_mwh = 120.0;

  return exogenous;
}

[[nodiscard]] auto make_default_state_grid() -> StateGrid {
  return StateGrid{
      std::vector<double>{0.0},
      std::vector<double>{0.0, 1.0},
  };
}

[[nodiscard]] auto make_default_action_grid() -> ActionGrid {
  return ActionGrid::from_axes(ActionAxes{
      .turbine_flow_m3_s = {0.0},
      .spill_flow_m3_s = {0.0},
      .pump_flow_m3_s = {0.0},
      .battery_charge_mw = {0.0, 1.0},
      .battery_discharge_mw = {0.0, 1.0},
  });
}

[[nodiscard]] auto make_default_initial_state() -> State {
  return State{
      .reservoir_volume_m3 = 0.0,
      .battery_soc_mwh = 0.0,
  };
}
  
[[nodiscard]] auto run_dispatch(const std::vector<Exogenous>& exogenous) -> SimulationResult {
  const auto parameters = make_default_parameters();
  const auto state_grid = make_default_state_grid();
  const auto action_grid = make_default_action_grid();

  const PumpedStorageModel model{parameters};
  const BellmanSolver solver{model, state_grid, action_grid};
  const auto optimization_result = solver.solve(exogenous);

  const ForwardSimulator simulator{model, state_grid};
  return simulator.simulate(make_default_initial_state(), exogenous, optimization_result.policy);
}



[[nodiscard]] auto run_default_dispatch() -> SimulationResult {
  return run_dispatch(make_default_exogenous());
}

[[nodiscard]] auto scenario_to_json(const std::vector<Exogenous>& exogenous,
                                    const ModelParameters& parameters) -> std::string {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6);
  stream << '{';
  stream << "\"name\":\"sample_day_ahead_arbitrage\",";
  stream << "\"description\":\"Small deterministic pumped-storage and battery dispatch scenario\",";
  stream << "\"timestep_hours\":" << parameters.timestep_hours << ',';
  stream << "\"initial_state\":";
  append_state_json(stream, make_default_initial_state());
  stream << ',';
  stream << "\"parameters\":{"
         << "\"hydro\":{"
         << "\"min_reservoir_volume_m3\":" << parameters.hydro.min_reservoir_volume_m3 << ','
         << "\"max_reservoir_volume_m3\":" << parameters.hydro.max_reservoir_volume_m3 << ','
         << "\"max_turbine_flow_m3_s\":" << parameters.hydro.max_turbine_flow_m3_s << ','
         << "\"max_pump_flow_m3_s\":" << parameters.hydro.max_pump_flow_m3_s << ','
         << "\"hydraulic_head_m\":" << parameters.hydro.hydraulic_head_m << "},"
         << "\"battery\":{"
         << "\"enabled\":" << (parameters.battery.enabled ? "true" : "false") << ','
         << "\"capacity_mwh\":" << parameters.battery.capacity_mwh << ','
         << "\"max_charge_mw\":" << parameters.battery.max_charge_mw << ','
         << "\"max_discharge_mw\":" << parameters.battery.max_discharge_mw << "}"
         << "},";
  stream << "\"exogenous\":[";
  for (std::size_t index = 0; index < exogenous.size(); ++index) {
    if (index != 0U) {
      stream << ',';
    }
    stream << "{\"time_index\":" << index << ',';
    stream << "\"price_eur_per_mwh\":" << exogenous[index].price_eur_per_mwh << ',';
    stream << "\"natural_inflow_m3_s\":" << exogenous[index].natural_inflow_m3_s << '}';
  }
  stream << "]}";
  return stream.str();
}

[[nodiscard]] auto simulation_to_json(const SimulationResult& result) -> std::string {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6);
  stream << '{';
  stream << "\"run\":{\"status\":\"completed\",\"total_profit_eur\":" << result.total_profit_eur
         << ",\"final_state\":";
  append_state_json(stream, result.final_state);
  stream << "},\"steps\":[";

  for (std::size_t index = 0; index < result.steps.size(); ++index) {
    if (index != 0U) {
      stream << ',';
    }
    const auto& step = result.steps[index];
    stream << "{\"time_index\":" << step.time_index << ',';
    stream << "\"state\":";
    append_state_json(stream, step.state);
    stream << ",\"action\":";
    append_action_json(stream, step.action);
    stream << ",\"exogenous\":";
    append_exogenous_json(stream, step.exogenous);
    stream << ",\"outcome\":";
    append_outcome_json(stream, step.outcome);
    stream << ",\"cumulative_profit_eur\":" << step.cumulative_profit_eur << '}';
  }

  stream << "]}";
  return stream.str();
}

[[nodiscard]] auto simulation_to_csv(const SimulationResult& result) -> std::string {
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(6);
  stream << "time,price,reservoir_m3,battery_mwh,turbine_flow,pump_flow,battery_charge,battery_discharge,net_power,reward,cumulative_profit\n";
  for (const auto& step : result.steps) {
    stream << step.time_index << ','
           << step.exogenous.price_eur_per_mwh << ','
           << step.state.reservoir_volume_m3 << ','
           << step.state.battery_soc_mwh << ','
           << step.action.turbine_flow_m3_s << ','
           << step.action.pump_flow_m3_s << ','
           << step.action.battery_charge_mw << ','
           << step.action.battery_discharge_mw << ','
           << step.outcome.net_power_mw << ','
           << step.outcome.reward_eur << ','
           << step.cumulative_profit_eur << '\n';
  }
  stream << "total_profit_eur," << result.total_profit_eur << '\n';
  return stream.str();
}

}  // namespace optiflow::demo
