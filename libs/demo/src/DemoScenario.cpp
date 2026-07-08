#include "optiflow/demo/DemoScenario.h"

#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/stochastic/StochasticBellmanSolver.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace optiflow::demo {
namespace {

[[nodiscard]] auto make_axis(std::initializer_list<double> values) -> std::vector<double> {
  std::vector<double> axis;
  axis.reserve(values.size());
  for (const auto value : values) {
    if (value >= 0.0 && std::isfinite(value)) {
      axis.push_back(value);
    }
  }
  std::sort(axis.begin(), axis.end());
  axis.erase(std::unique(axis.begin(), axis.end(), [](const double lhs, const double rhs) {
               return std::abs(lhs - rhs) < 1.0e-9;
             }),
             axis.end());
  if (axis.empty() || std::abs(axis.front()) > 1.0e-9) {
    axis.insert(axis.begin(), 0.0);
  }
  return axis;
}

[[nodiscard]] auto make_state_grid_for_parameters(const ModelParameters& parameters) -> StateGrid {
  const auto min_volume = parameters.hydro.min_reservoir_volume_m3;
  const auto max_volume = parameters.hydro.max_reservoir_volume_m3;
  const auto span = max_volume - min_volume;

  auto battery_axis = std::vector<double>{0.0};
  if (parameters.battery.enabled) {
    battery_axis = make_axis({0.0, 0.5 * parameters.battery.capacity_mwh, parameters.battery.capacity_mwh});
  }

  return StateGrid{
      std::vector<double>{min_volume,
                          min_volume + 0.25 * span,
                          min_volume + 0.50 * span,
                          min_volume + 0.75 * span,
                          max_volume},
      battery_axis,
  };
}

[[nodiscard]] auto make_action_grid_for_parameters(const ModelParameters& parameters) -> ActionGrid {
  const auto turbine_axis = make_axis({0.0,
                                       0.5 * parameters.hydro.max_turbine_flow_m3_s,
                                       parameters.hydro.max_turbine_flow_m3_s});
  const auto spill_axis = make_axis({0.0,
                                    0.5 * parameters.hydro.max_spill_flow_m3_s,
                                    parameters.hydro.max_spill_flow_m3_s});
  const auto pump_axis = make_axis({0.0,
                                   0.5 * parameters.hydro.max_pump_flow_m3_s,
                                   parameters.hydro.max_pump_flow_m3_s});

  const auto charge_axis = parameters.battery.enabled
      ? make_axis({0.0, 0.5 * parameters.battery.max_charge_mw, parameters.battery.max_charge_mw})
      : std::vector<double>{0.0};
  const auto discharge_axis = parameters.battery.enabled
      ? make_axis({0.0, 0.5 * parameters.battery.max_discharge_mw, parameters.battery.max_discharge_mw})
      : std::vector<double>{0.0};

  return ActionGrid::from_axes(ActionAxes{
      .turbine_flow_m3_s = turbine_axis,
      .spill_flow_m3_s = spill_axis,
      .pump_flow_m3_s = pump_axis,
      .battery_charge_mw = charge_axis,
      .battery_discharge_mw = discharge_axis,
  });
}

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

void append_parameters_json(std::ostringstream& stream, const ModelParameters& parameters) {
  stream << "{\"timestep_hours\":" << parameters.timestep_hours
         << ",\"terminal_water_value_eur_per_m3\":" << parameters.terminal_water_value_eur_per_m3
         << ",\"terminal_battery_value_eur_per_mwh\":" << parameters.terminal_battery_value_eur_per_mwh
         << ",\"hydro\":{"
         << "\"min_reservoir_volume_m3\":" << parameters.hydro.min_reservoir_volume_m3 << ','
         << "\"max_reservoir_volume_m3\":" << parameters.hydro.max_reservoir_volume_m3 << ','
         << "\"max_turbine_flow_m3_s\":" << parameters.hydro.max_turbine_flow_m3_s << ','
         << "\"max_pump_flow_m3_s\":" << parameters.hydro.max_pump_flow_m3_s << ','
         << "\"max_spill_flow_m3_s\":" << parameters.hydro.max_spill_flow_m3_s << ','
         << "\"hydraulic_head_m\":" << parameters.hydro.hydraulic_head_m << ','
         << "\"turbine_efficiency\":" << parameters.hydro.turbine_efficiency << ','
         << "\"pump_efficiency\":" << parameters.hydro.pump_efficiency << ','
         << "\"turbine_cost_eur_per_mwh\":" << parameters.hydro.turbine_cost_eur_per_mwh << ','
         << "\"pump_cost_eur_per_mwh\":" << parameters.hydro.pump_cost_eur_per_mwh << ','
         << "\"spill_penalty_eur_per_m3\":" << parameters.hydro.spill_penalty_eur_per_m3
         << "},\"battery\":{"
         << "\"enabled\":" << (parameters.battery.enabled ? "true" : "false") << ','
         << "\"capacity_mwh\":" << parameters.battery.capacity_mwh << ','
         << "\"max_charge_mw\":" << parameters.battery.max_charge_mw << ','
         << "\"max_discharge_mw\":" << parameters.battery.max_discharge_mw << ','
         << "\"charge_efficiency\":" << parameters.battery.charge_efficiency << ','
         << "\"discharge_efficiency\":" << parameters.battery.discharge_efficiency << ','
         << "\"degradation_cost_eur_per_mwh\":" << parameters.battery.degradation_cost_eur_per_mwh
         << "}}";
}

void append_optimization_config_json(std::ostringstream& stream, const OptimizationConfig& config) {
  stream << "{\"discount_factor\":" << config.discount_factor
         << ",\"forbid_simultaneous_pump_and_turbine\":"
         << (config.forbid_simultaneous_pump_and_turbine ? "true" : "false")
         << ",\"forbid_simultaneous_charge_and_discharge\":"
         << (config.forbid_simultaneous_charge_and_discharge ? "true" : "false") << '}';
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

[[nodiscard]] auto make_expected_exogenous_path(const StochasticExogenousProcess& process) -> std::vector<Exogenous> {
  if (process.empty()) {
    throw std::invalid_argument{"stochastic process must contain at least one time step"};
  }

  std::vector<Exogenous> expected_path;
  expected_path.reserve(process.size());

  for (const auto& distribution : process) {
    if (distribution.empty()) {
      throw std::invalid_argument{"each stochastic stage must contain at least one realization"};
    }

    Exogenous expected{};
    for (const auto& realization : distribution) {
      expected.price_eur_per_mwh += realization.probability * realization.value.price_eur_per_mwh;
      expected.natural_inflow_m3_s += realization.probability * realization.value.natural_inflow_m3_s;
    }
    expected_path.push_back(expected);
  }

  return expected_path;
}

}  // namespace

[[nodiscard]] auto make_default_parameters() -> ModelParameters {
  return ModelParameters{
      .hydro = HydroParameters{
          .min_reservoir_volume_m3 = 0.0,
          .max_reservoir_volume_m3 = 100'000'000.0,
          .max_turbine_flow_m3_s = 150.0,
          .max_pump_flow_m3_s = 75.0,
          .max_spill_flow_m3_s = 260.0,
          .hydraulic_head_m = 120.0,
          .turbine_efficiency = 0.90,
          .pump_efficiency = 0.85,
          .turbine_cost_eur_per_mwh = 1.0,
          .pump_cost_eur_per_mwh = 0.5,
          .spill_penalty_eur_per_m3 = 0.0,
      },
      .battery = BatteryParameters{
          .enabled = true,
          .capacity_mwh = 50.0,
          .max_charge_mw = 25.0,
          .max_discharge_mw = 25.0,
          .charge_efficiency = 0.95,
          .discharge_efficiency = 0.95,
          .degradation_cost_eur_per_mwh = 1.0,
      },
      .timestep_hours = 1.0,
      .terminal_water_value_eur_per_m3 = 0.001,
      .terminal_battery_value_eur_per_mwh = 5.0,
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
  return make_state_grid_for_parameters(make_default_parameters());
}

[[nodiscard]] auto make_default_action_grid() -> ActionGrid {
  return make_action_grid_for_parameters(make_default_parameters());
}

[[nodiscard]] auto make_default_initial_state() -> State {
  return State{
      .reservoir_volume_m3 = 50'000'000.0,
      .battery_soc_mwh = 25.0,
  };
}

[[nodiscard]] auto make_default_optimization_config() -> OptimizationConfig {
  return OptimizationConfig{};
}

[[nodiscard]] auto run_dispatch(const std::vector<Exogenous>& exogenous) -> SimulationResult {
  return run_dispatch(exogenous, make_default_parameters(), make_default_initial_state(), make_default_optimization_config());
}

[[nodiscard]] auto run_dispatch(const std::vector<Exogenous>& exogenous,
                                const ModelParameters& parameters,
                                const State initial_state,
                                const OptimizationConfig config) -> SimulationResult {
  const auto state_grid = make_state_grid_for_parameters(parameters);
  const auto action_grid = make_action_grid_for_parameters(parameters);

  const PumpedStorageModel model{parameters};
  const BellmanSolver solver{model, state_grid, action_grid, config};
  const auto optimization_result = solver.solve(exogenous);

  const ForwardSimulator simulator{model, state_grid};
  return simulator.simulate(initial_state, exogenous, optimization_result.policy);
}

[[nodiscard]] auto run_stochastic_dispatch(const StochasticExogenousProcess& process) -> SimulationResult {
  return run_stochastic_dispatch(process, make_default_parameters(), make_default_initial_state(), make_default_optimization_config());
}

[[nodiscard]] auto run_stochastic_dispatch(const StochasticExogenousProcess& process,
                                           const ModelParameters& parameters,
                                           const State initial_state,
                                           const OptimizationConfig config) -> SimulationResult {
  const auto state_grid = make_state_grid_for_parameters(parameters);
  const auto action_grid = make_action_grid_for_parameters(parameters);

  const PumpedStorageModel model{parameters};
  const StochasticBellmanSolver solver{model, state_grid, action_grid, config};
  const auto optimization_result = solver.solve(process);

  const auto expected_path = make_expected_exogenous_path(process);
  const ForwardSimulator simulator{model, state_grid};
  return simulator.simulate(initial_state, expected_path, optimization_result.policy);
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
  stream << "\"solver_kind\":\"deterministic\",";
  stream << "\"timestep_hours\":" << parameters.timestep_hours << ',';
  stream << "\"initial_state\":";
  append_state_json(stream, make_default_initial_state());
  stream << ",\"parameters\":";
  append_parameters_json(stream, parameters);
  stream << ",\"optimization_config\":";
  append_optimization_config_json(stream, make_default_optimization_config());
  stream << ",\"exogenous\":[";
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
