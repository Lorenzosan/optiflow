#include "optiflow/core/StorageTypes.h"
#include "optiflow/demo/CsvScenarioLoader.h"
#include "optiflow/demo/DemoScenario.h"
#include "optiflow/demo/OptimizationRequest.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"
#include "optiflow/stochastic/StochasticBellmanSolver.h"

#include <cmath>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-6;
constexpr double grid_flow_m3_s = 69.4444444444;

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error{message};
  }
}

[[nodiscard]] auto near(const double lhs, const double rhs, const double eps = tolerance) -> bool {
  return std::abs(lhs - rhs) <= eps;
}

[[nodiscard]] auto make_parameters(const bool battery_enabled = true) -> optiflow::ModelParameters {
  return optiflow::ModelParameters{
      .hydro = optiflow::HydroParameters{
          .min_reservoir_volume_m3 = 0.0,
          .max_reservoir_volume_m3 = 1'000'000.0,
          .max_turbine_flow_m3_s = grid_flow_m3_s,
          .max_pump_flow_m3_s = grid_flow_m3_s,
          .max_spill_flow_m3_s = 100.0,
          .hydraulic_head_m = 120.0,
          .turbine_efficiency = 0.90,
          .pump_efficiency = 0.85,
          .turbine_cost_eur_per_mwh = 1.0,
          .pump_cost_eur_per_mwh = 0.5,
          .spill_penalty_eur_per_m3 = 0.0,
      },
      .battery = optiflow::BatteryParameters{
          .enabled = battery_enabled,
          .capacity_mwh = battery_enabled ? 50.0 : 0.0,
          .max_charge_mw = battery_enabled ? 25.0 : 0.0,
          .max_discharge_mw = battery_enabled ? 25.0 : 0.0,
          .charge_efficiency = 1.0,
          .discharge_efficiency = 1.0,
          .degradation_cost_eur_per_mwh = 1.0,
      },
      .timestep_hours = 1.0,
      .terminal_water_value_eur_per_m3 = 0.001,
      .terminal_battery_value_eur_per_mwh = 5.0,
  };
}

[[nodiscard]] auto make_state_grid(const bool battery_enabled = true) -> optiflow::StateGrid {
  return optiflow::StateGrid{
      std::vector<double>{0.0, 250'000.0, 500'000.0, 750'000.0, 1'000'000.0},
      battery_enabled ? std::vector<double>{0.0, 25.0, 50.0} : std::vector<double>{0.0},
  };
}

[[nodiscard]] auto make_action_grid(const bool battery_enabled = true) -> optiflow::ActionGrid {
  return optiflow::ActionGrid::from_axes(optiflow::ActionAxes{
      .turbine_flow_m3_s = {0.0, grid_flow_m3_s},
      .spill_flow_m3_s = {0.0},
      .pump_flow_m3_s = {0.0, grid_flow_m3_s},
      .battery_charge_mw = battery_enabled ? std::vector<double>{0.0, 25.0} : std::vector<double>{0.0},
      .battery_discharge_mw = battery_enabled ? std::vector<double>{0.0, 25.0} : std::vector<double>{0.0},
  });
}

[[nodiscard]] auto solve_and_simulate(const std::vector<optiflow::Exogenous>& exogenous,
                                      const optiflow::State initial_state,
                                      const bool battery_enabled = true) -> optiflow::SimulationResult {
  const auto parameters = make_parameters(battery_enabled);
  const auto state_grid = make_state_grid(battery_enabled);
  const auto action_grid = make_action_grid(battery_enabled);
  const optiflow::PumpedStorageModel model{parameters};
  const optiflow::BellmanSolver solver{model, state_grid, action_grid};
  const auto optimization_result = solver.solve(exogenous);
  const optiflow::ForwardSimulator simulator{model, state_grid};
  return simulator.simulate(initial_state, exogenous, optimization_result.policy);
}

void check_simulation_invariants(const optiflow::SimulationResult& result,
                                 const optiflow::ModelParameters& parameters) {
  double cumulative_profit = 0.0;
  const optiflow::PumpedStorageModel model{parameters};

  for (const auto& step : result.steps) {
    require(step.outcome.feasible, "all simulated outcomes must be feasible");
    require(model.is_state_feasible(step.state), "all simulated states must be feasible");
    require(model.is_state_feasible(step.outcome.next_state), "all simulated next states must be feasible");

    cumulative_profit += step.outcome.reward_eur;
    require(near(cumulative_profit, step.cumulative_profit_eur), "cumulative profit must equal the sum of rewards");

    require(!(step.action.pump_flow_m3_s > tolerance && step.action.turbine_flow_m3_s > tolerance),
            "pump and turbine must not both be active");
    require(!(step.action.battery_charge_mw > tolerance && step.action.battery_discharge_mw > tolerance),
            "battery charge and discharge must not both be active");
  }

  require(near(cumulative_profit, result.total_profit_eur), "total profit must equal the final cumulative profit");
}

void test_peak_price_arbitrage() {
  const std::vector<optiflow::Exogenous> exogenous{
      {.price_eur_per_mwh = 15.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = -20.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 10.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 120.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 90.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 20.0, .natural_inflow_m3_s = 0.0},
  };

  const auto result = solve_and_simulate(
      exogenous,
      optiflow::State{.reservoir_volume_m3 = 500'000.0, .battery_soc_mwh = 25.0});

  check_simulation_invariants(result, make_parameters(true));

  auto high_price_export_seen = false;
  auto negative_price_import_seen = false;

  for (const auto& step : result.steps) {
    if (step.exogenous.price_eur_per_mwh >= 90.0 && step.outcome.net_power_mw > tolerance) {
      high_price_export_seen = true;
    }
    if (step.exogenous.price_eur_per_mwh < 0.0 && step.outcome.net_power_mw < -tolerance) {
      negative_price_import_seen = true;
    }
  }

  require(high_price_export_seen, "complex arbitrage scenario should export during high prices");
  require(negative_price_import_seen, "complex arbitrage scenario should import during negative prices");
  require(result.total_profit_eur > 0.0, "complex arbitrage scenario should be profitable");
}

void test_inflow_boundary_pressure() {
  const std::vector<optiflow::Exogenous> exogenous{
      {.price_eur_per_mwh = 5.0, .natural_inflow_m3_s = grid_flow_m3_s},
      {.price_eur_per_mwh = 5.0, .natural_inflow_m3_s = grid_flow_m3_s},
      {.price_eur_per_mwh = 100.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 100.0, .natural_inflow_m3_s = 0.0},
  };

  const auto result = solve_and_simulate(
      exogenous,
      optiflow::State{.reservoir_volume_m3 = 750'000.0, .battery_soc_mwh = 25.0});

  check_simulation_invariants(result, make_parameters(true));

  for (const auto& step : result.steps) {
    require(step.outcome.next_state.reservoir_volume_m3 <= 1'000'000.0 + tolerance,
            "reservoir must never exceed the maximum volume");
  }
}

void test_hydro_only_mode() {
  const std::vector<optiflow::Exogenous> exogenous{
      {.price_eur_per_mwh = -30.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 10.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 130.0, .natural_inflow_m3_s = 0.0},
      {.price_eur_per_mwh = 130.0, .natural_inflow_m3_s = 0.0},
  };

  const auto result = solve_and_simulate(
      exogenous,
      optiflow::State{.reservoir_volume_m3 = 500'000.0, .battery_soc_mwh = 0.0},
      false);

  check_simulation_invariants(result, make_parameters(false));

  for (const auto& step : result.steps) {
    require(near(step.action.battery_charge_mw, 0.0), "battery charge must be zero when battery is disabled");
    require(near(step.action.battery_discharge_mw, 0.0), "battery discharge must be zero when battery is disabled");
    require(near(step.outcome.next_state.battery_soc_mwh, 0.0), "battery state must remain zero when battery is disabled");
  }
}



void test_separated_csv_loader() {
  const auto directory = std::filesystem::temp_directory_path();
  const auto price_path = directory / "optiflow_test_prices.csv";
  const auto inflow_path = directory / "optiflow_test_inflows.csv";

  {
    std::ofstream prices{price_path};
    prices << "time_index,price_eur_per_mwh\n"
           << "0,20.0\n"
           << "1,-10.0\n"
           << "2,80.0\n";
  }

  {
    std::ofstream inflows{inflow_path};
    inflows << "time_index,natural_inflow_m3_s\n"
            << "0,0.0\n"
            << "1,2.5\n"
            << "2,0.0\n";
  }

  const auto exogenous = optiflow::demo::load_deterministic_exogenous_csv(
      price_path.string(),
      inflow_path.string());

  std::filesystem::remove(price_path);
  std::filesystem::remove(inflow_path);

  require(exogenous.size() == 3U, "CSV loader must create one exogenous value per time step");
  require(near(exogenous[0].price_eur_per_mwh, 20.0), "CSV loader must read prices from the price file");
  require(near(exogenous[1].natural_inflow_m3_s, 2.5), "CSV loader must read inflows from the inflow file");
  require(near(exogenous[2].price_eur_per_mwh, 80.0), "CSV loader must preserve time order");
}


void test_separated_stochastic_csv_loader() {
  const auto directory = std::filesystem::temp_directory_path();
  const auto price_path = directory / "optiflow_test_stochastic_prices.csv";
  const auto inflow_path = directory / "optiflow_test_stochastic_inflows.csv";

  {
    std::ofstream prices{price_path};
    prices << "time_index,realization_index,probability,price_eur_per_mwh\n"
           << "0,0,0.25,10.0\n"
           << "0,1,0.75,90.0\n"
           << "1,0,0.40,20.0\n"
           << "1,1,0.60,120.0\n";
  }

  {
    std::ofstream inflows{inflow_path};
    inflows << "time_index,realization_index,probability,natural_inflow_m3_s\n"
            << "0,0,0.25,0.0\n"
            << "0,1,0.75,4.0\n"
            << "1,0,0.40,2.0\n"
            << "1,1,0.60,0.0\n";
  }

  const auto process = optiflow::demo::load_stochastic_exogenous_csv(
      price_path.string(),
      inflow_path.string());

  std::filesystem::remove(price_path);
  std::filesystem::remove(inflow_path);

  require(process.size() == 2U, "stochastic CSV loader must create one distribution per time step");
  require(process[0].size() == 2U, "stochastic CSV loader must preserve realizations at time zero");
  require(process[1].size() == 2U, "stochastic CSV loader must preserve realizations at time one");
  require(near(process[0][0].probability, 0.25), "stochastic CSV loader must read probabilities");
  require(near(process[0][1].value.price_eur_per_mwh, 90.0), "stochastic CSV loader must read price realizations");
  require(near(process[0][1].value.natural_inflow_m3_s, 4.0), "stochastic CSV loader must join inflow realizations by key");
  require(near(process[1][0].value.natural_inflow_m3_s, 2.0), "stochastic CSV loader must preserve time order");
}

void test_stochastic_solver_probability_validation_and_policy() {
  const auto parameters = make_parameters(true);
  const auto state_grid = make_state_grid(true);
  const auto action_grid = make_action_grid(true);
  const optiflow::PumpedStorageModel model{parameters};
  const optiflow::StochasticBellmanSolver solver{model, state_grid, action_grid};

  const optiflow::StochasticExogenousProcess process{
      optiflow::StageDistribution{
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 10.0, .natural_inflow_m3_s = 0.0}, .probability = 0.5},
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 100.0, .natural_inflow_m3_s = 0.0}, .probability = 0.5},
      },
      optiflow::StageDistribution{
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 120.0, .natural_inflow_m3_s = 0.0}, .probability = 1.0},
      },
  };

  const auto result = solver.solve(process);
  const auto action = result.policy.action_at(0U, optiflow::StateIndex{.reservoir_index = 2U, .battery_index = 1U});
  require(action.has_value(), "stochastic solver must produce a policy action");
}

void test_demo_stochastic_dispatch_uses_expected_path() {
  const optiflow::StochasticExogenousProcess process{
      optiflow::StageDistribution{
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 10.0, .natural_inflow_m3_s = 0.0}, .probability = 0.25},
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 50.0, .natural_inflow_m3_s = 0.0}, .probability = 0.75},
      },
      optiflow::StageDistribution{
          optiflow::WeightedExogenous{.value = optiflow::Exogenous{.price_eur_per_mwh = 100.0, .natural_inflow_m3_s = 0.0}, .probability = 1.0},
      },
  };

  const auto result = optiflow::demo::run_stochastic_dispatch(process);

  require(result.steps.size() == 2U, "demo stochastic dispatch must produce one step per stochastic stage");
  require(near(result.steps[0].exogenous.price_eur_per_mwh, 40.0),
          "demo stochastic dispatch must forward-simulate with the expected price path");
  require(near(result.steps[0].exogenous.natural_inflow_m3_s, 0.0),
          "demo stochastic dispatch must forward-simulate with the expected inflow path");
}

void test_parse_deterministic_optimization_request_json() {
  const auto request = optiflow::demo::parse_optimization_request_json(R"json({
    "solver_kind": "deterministic",
    "exogenous": [
      {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0},
      {"time_index": 1, "price_eur_per_mwh": -10.0, "natural_inflow_m3_s": 2.5}
    ]
  })json");

  require(request.solver_kind == optiflow::demo::RequestSolverKind::Deterministic,
          "request parser must read deterministic solver kind");
  require(request.exogenous.size() == 2U, "request parser must read deterministic exogenous inputs");
  require(near(request.exogenous[0].price_eur_per_mwh, 20.0), "request parser must read deterministic prices");
  require(near(request.exogenous[1].natural_inflow_m3_s, 2.5), "request parser must read deterministic inflows");
}

void test_parse_stochastic_optimization_request_json() {
  const auto request = optiflow::demo::parse_optimization_request_json(R"json({
    "solver_kind": "stochastic",
    "stochastic_process": [
      {"time_index": 0, "realizations": [
        {"realization_index": 0, "probability": 0.25, "price_eur_per_mwh": 10.0, "natural_inflow_m3_s": 0.0},
        {"realization_index": 1, "probability": 0.75, "price_eur_per_mwh": 90.0, "natural_inflow_m3_s": 4.0}
      ]}
    ]
  })json");

  require(request.solver_kind == optiflow::demo::RequestSolverKind::Stochastic,
          "request parser must read stochastic solver kind");
  require(request.stochastic_process.size() == 1U, "request parser must read stochastic stages");
  require(request.stochastic_process[0].size() == 2U, "request parser must read stochastic realizations");
  require(near(request.stochastic_process[0][1].probability, 0.75), "request parser must read probabilities");
  require(near(request.stochastic_process[0][1].value.price_eur_per_mwh, 90.0), "request parser must read stochastic prices");
  require(near(request.stochastic_process[0][1].value.natural_inflow_m3_s, 4.0), "request parser must read stochastic inflows");
}

void test_parse_empty_optimization_request_uses_default_scenario() {
  const auto request = optiflow::demo::parse_optimization_request_json("{}");

  require(request.solver_kind == optiflow::demo::RequestSolverKind::Deterministic,
          "empty request parser must default to deterministic mode");
  require(!request.exogenous.empty(), "empty request parser must use the default deterministic scenario");
}

void test_parse_invalid_optimization_request_rejects_bad_time_index() {
  auto failed = false;
  try {
    static_cast<void>(optiflow::demo::parse_optimization_request_json(R"json({
      "solver_kind": "deterministic",
      "exogenous": [
        {"time_index": 1, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0}
      ]
    })json"));
  } catch (const std::invalid_argument&) {
    failed = true;
  }

  require(failed, "request parser must reject non-contiguous deterministic time indices");
}

void test_longer_horizon_stress() {
  std::vector<optiflow::Exogenous> exogenous;
  exogenous.reserve(48U);

  for (std::size_t hour = 0U; hour < 48U; ++hour) {
    const auto in_morning_peak = hour % 24U >= 7U && hour % 24U <= 9U;
    const auto in_evening_peak = hour % 24U >= 18U && hour % 24U <= 21U;
    const auto in_night = hour % 24U <= 5U;
    const auto price = in_evening_peak ? 120.0 : in_morning_peak ? 85.0 : in_night ? -10.0 : 30.0;
    const auto inflow = hour % 12U == 0U ? grid_flow_m3_s : 0.0;
    exogenous.push_back(optiflow::Exogenous{.price_eur_per_mwh = price, .natural_inflow_m3_s = inflow});
  }

  const auto result = solve_and_simulate(
      exogenous,
      optiflow::State{.reservoir_volume_m3 = 500'000.0, .battery_soc_mwh = 25.0});

  check_simulation_invariants(result, make_parameters(true));
  require(result.steps.size() == exogenous.size(), "stress scenario must produce one step per exogenous point");
}

}  // namespace

int main() {
  try {
    test_peak_price_arbitrage();
    test_inflow_boundary_pressure();
    test_hydro_only_mode();
    test_separated_csv_loader();
    test_separated_stochastic_csv_loader();
    test_stochastic_solver_probability_validation_and_policy();
    test_demo_stochastic_dispatch_uses_expected_path();
    test_parse_deterministic_optimization_request_json();
    test_parse_stochastic_optimization_request_json();
    test_parse_empty_optimization_request_uses_default_scenario();
    test_parse_invalid_optimization_request_rejects_bad_time_index();
    test_longer_horizon_stress();
  } catch (const std::exception& error) {
    std::cerr << "test failure: " << error.what() << '\n';
    return 1;
  }

  std::cout << "all scenario tests passed\n";
  return 0;
}
