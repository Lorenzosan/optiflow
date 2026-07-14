#include "optiflow/core/CsvScenarioReader.h"
#include "optiflow/core/Scenario.h"
#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/Interpolator.h"
#include "optiflow/numerics/Policy.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/numerics/ValueFunction.h"
#include "optiflow/runner/OptimizationRunner.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
namespace core = optiflow::core;
namespace model = optiflow::model;
namespace numerics = optiflow::numerics;
namespace solver = optiflow::solver;
constexpr double tolerance = 1.0e-9;

void require(bool condition, std::string_view message) {
    if (!condition) throw std::runtime_error(std::string(message));
}
void near(double actual, double expected, std::string_view message) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(std::string(message));
    }
}
template <typename Exception, typename Function>
void require_throws(Function&& function, std::string_view fragment) {
    try {
        function();
    } catch (const Exception& error) {
        require(std::string(error.what()).find(fragment) != std::string::npos, "wrong error text");
        return;
    }
    throw std::runtime_error("expected exception");
}

core::ModelParameters model_parameters() {
    return core::ModelParameters(1.0, 0.0, 100.0, 20.0, 10.0, 20.0,
                                 0.9, 0.85, 1.0);
}
core::TerminalParameters open_terminal() {
    return core::TerminalParameters(0.0, 100.0, 50.0, 0.0);
}
core::SolverParameters solver_parameters() {
    return core::SolverParameters(11, 3, 1, 2, 1.0);
}

void test_transition_and_reward() {
    const model::PumpedStorageModel pumped_storage(model_parameters());
    const core::Outcome outcome = pumped_storage.apply(
        core::State(50.0), core::Action(10.0, 0.0, 0.0), core::Exogenous(100.0, 2.0));
    require(outcome.feasible, "transition feasible");
    near(outcome.next_state.reservoir_volume, 42.0, "next volume");
    near(outcome.turbine_power, 3.6, "turbine power");
    near(outcome.net_power, 3.6, "net power");
    near(outcome.reward, 356.4, "reward");
}

void test_zero_reward_is_canonical_positive_zero() {
    const model::PumpedStorageModel pumped_storage(model_parameters());
    const core::Outcome outcome = pumped_storage.apply(
        core::State(95.0), core::Action(0.0, 5.0, 0.0), core::Exogenous(-10.0, 5.0));
    require(outcome.feasible, "zero-reward spill transition feasible");
    require(outcome.reward == 0.0, "zero-reward spill has zero reward");
    require(!std::signbit(outcome.reward), "zero reward is not negative zero");
}

void test_mutual_exclusion_and_bounds() {
    const model::PumpedStorageModel pumped_storage(model_parameters());
    const core::Outcome simultaneous = pumped_storage.apply(
        core::State(50.0), core::Action(10.0, 0.0, 5.0), core::Exogenous(0.0, 0.0));
    require(!simultaneous.feasible, "simultaneous turbine and pump rejected");
    const core::Outcome outside = pumped_storage.apply(
        core::State(5.0), core::Action(20.0, 0.0, 0.0), core::Exogenous(0.0, 0.0));
    require(!outside.feasible, "outside state rejected");
}

void test_action_grid_contains_only_unique_feasible_controls() {
    const numerics::ActionGrid grid = numerics::ActionGrid::from_parameters(
        model_parameters(), core::SolverParameters(11, 3, 1, 2, 1.0));
    require(grid.size() == 4, "mutually exclusive controls are filtered");
    for (const core::Action& action : grid.actions()) {
        require(!(action.turbine_flow > 0.0 && action.pump_flow > 0.0),
                "action grid contains simultaneous turbine and pump flow");
    }

    const core::ModelParameters no_pumping(
        1.0, 0.0, 100.0, 20.0, 0.0, 0.0, 0.9, 0.85, 1.0);
    const numerics::ActionGrid zero_range_grid = numerics::ActionGrid::from_parameters(
        no_pumping, core::SolverParameters(11, 3, 1, 3, 1.0));
    require(zero_range_grid.size() == 3, "zero-range control axis is collapsed");
}

void test_state_grid_and_linear_interpolation() {
    const numerics::StateGrid grid(0.0, 10.0, 3);
    near(grid.reservoir_volume_at(1), 5.0, "grid coordinate");
    require(grid.nearest_index(core::State(8.0)).reservoir_index == 2, "nearest index");

    numerics::ValueFunction values(1, grid);
    values.set(1, numerics::StateIndex(0), 0.0);
    values.set(1, numerics::StateIndex(1), 10.0);
    values.set(1, numerics::StateIndex(2), 20.0);
    near(numerics::Interpolator::linear(values, grid, 1, core::State(7.5)), 15.0,
         "linear interpolation");

    numerics::Policy policy(1, grid);
    policy.set(0, numerics::StateIndex(1), core::Action(3.0, 0.0, 0.0));
    near(policy.get(0, numerics::StateIndex(1)).turbine_flow, 3.0, "policy action");
}

void test_scenario_validation() {
    require_throws<std::invalid_argument>([] {
        const core::Scenario scenario("bad", core::State(200.0),
                                      {core::Exogenous(0.0, 0.0)},
                                      model_parameters(), open_terminal());
        static_cast<void>(scenario);
    }, "initial_reservoir_volume");
    require_throws<std::invalid_argument>([] {
        const core::Scenario scenario("bad", core::State(50.0),
                                      {core::Exogenous(0.0, -1.0)},
                                      model_parameters(), open_terminal());
        static_cast<void>(scenario);
    }, "natural_inflow");
    require_throws<std::invalid_argument>([] {
        const core::Scenario scenario("grid", core::State(50.0),
                                      {core::Exogenous(0.0, 0.0)},
                                      model_parameters(), open_terminal());
        const core::ScenarioBundle bundle(scenario, core::SolverParameters(1, 1, 1, 1, 1.0));
        static_cast<void>(bundle);
    }, "reservoir_volume_grid_points");
}

void test_bellman_forward_and_runner() {
    const core::Scenario scenario(
        "unit",
        core::State(50.0),
        {core::Exogenous(30.0, 2.0), core::Exogenous(100.0, 2.0)},
        model_parameters(),
        open_terminal());
    const core::ScenarioBundle bundle(scenario, solver_parameters());
    const optiflow::runner::OptimizationResult result =
        optiflow::runner::OptimizationRunner().run(bundle);
    require(result.dispatch.size() == 2, "runner trajectory size");
    require(result.diagnostics.reservoir_grid_points == 11, "runner grid diagnostic");
    require(result.diagnostics.action_count == 4, "runner action diagnostic");
    near(result.diagnostics.final_reservoir_volume,
         result.dispatch.back().next_state.reservoir_volume,
         "final reservoir diagnostic");
}

void write_valid_scenario(const std::filesystem::path& path, double time_step_hours = 1.0) {
    std::ofstream scenario(path);
    scenario << "key,value\n"
             << "scenario_name,timestamp_case\n"
             << "time_step_hours," << time_step_hours << "\n"
             << "reservoir_min_volume,0\n"
             << "reservoir_max_volume,100\n"
             << "turbine_max_flow,20\n"
             << "pump_max_flow,10\n"
             << "spill_max_flow,20\n"
             << "turbine_efficiency,0.9\n"
             << "pump_efficiency,0.85\n"
             << "operating_cost_per_mwh,1\n"
             << "initial_reservoir_volume,50\n"
             << "terminal_reservoir_min_volume,0\n"
             << "terminal_reservoir_max_volume,100\n"
             << "terminal_target_reservoir_volume,50\n"
             << "terminal_reservoir_target_penalty,0\n"
             << "reservoir_volume_grid_points,11\n"
             << "turbine_flow_steps,3\n"
             << "spill_flow_steps,1\n"
             << "pump_flow_steps,2\n"
             << "discount_factor,1\n";
}

void write_series(const std::filesystem::path& path,
                  const char* value_column,
                  const std::vector<std::pair<std::string, double>>& rows) {
    std::ofstream output(path);
    output << "timestamp_utc," << value_column << '\n';
    for (const auto& [timestamp, value] : rows) {
        output << timestamp << ',' << value << '\n';
    }
}

void test_csv_reader_preserves_and_validates_timestamps() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto scenario_path = directory / "optiflow_timestamp_scenario.csv";
    const auto prices_path = directory / "optiflow_timestamp_prices.csv";
    const auto inflows_path = directory / "optiflow_timestamp_inflows.csv";
    write_valid_scenario(scenario_path);
    write_series(prices_path, "price", {
        {"2027-01-01T00:00:00Z", 10.0},
        {"2027-01-01T01:00:00Z", 20.0},
    });
    write_series(inflows_path, "natural_inflow", {
        {"2027-01-01T00:00:00Z", 0.0},
        {"2027-01-01T01:00:00Z", 1.0},
    });

    const core::ScenarioBundle bundle =
        core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path);
    require(bundle.scenario.exogenous_series().at(1).timestamp_utc ==
                "2027-01-01T01:00:00Z",
            "CSV timestamp is preserved in the exogenous series");

    write_series(inflows_path, "natural_inflow", {
        {"2027-01-01T00:00:00Z", 0.0},
        {"2027-01-01T02:00:00Z", 1.0},
    });
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));
    }, "timestamp spacing");

    write_series(inflows_path, "natural_inflow", {
        {"2027-01-01T01:00:00Z", 0.0},
        {"2027-01-01T02:00:00Z", 1.0},
    });
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));
    }, "timestamp_utc values must match");

    write_series(inflows_path, "natural_inflow", {
        {"2027-01-01T00:00:00Z", 0.0},
        {"2027-01-01T01:00:00Z", 1.0},
    });
    write_series(prices_path, "price", {
        {"2027-01-01T00:00:00+00:00", 10.0},
    });
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));
    }, "YYYY-MM-DDTHH:MM:SSZ");

    std::filesystem::remove(scenario_path);
    std::filesystem::remove(prices_path);
    std::filesystem::remove(inflows_path);
}

void test_csv_reader_accepts_only_the_legacy_fixed_power_factor() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto scenario_path = directory / "optiflow_legacy_power_factor.csv";
    const auto prices_path = directory / "optiflow_legacy_power_factor_prices.csv";
    const auto inflows_path = directory / "optiflow_legacy_power_factor_inflows.csv";
    write_valid_scenario(scenario_path);
    {
        std::ofstream scenario(scenario_path, std::ios::app);
        scenario << "water_to_power_factor,0.4\n";
    }
    write_series(prices_path, "price", {{"2027-01-01T00:00:00Z", 0.0}});
    write_series(inflows_path, "natural_inflow", {{"2027-01-01T00:00:00Z", 0.0}});
    static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));

    write_valid_scenario(scenario_path);
    {
        std::ofstream scenario(scenario_path, std::ios::app);
        scenario << "water_to_power_factor,0.5\n";
    }
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));
    }, "legacy water_to_power_factor must be 0.4 or removed");

    std::filesystem::remove(scenario_path);
    std::filesystem::remove(prices_path);
    std::filesystem::remove(inflows_path);
}

void test_csv_reader_rejects_unsupported_key() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto scenario_path = directory / "optiflow_removed_storage.csv";
    const auto prices_path = directory / "optiflow_removed_storage_prices.csv";
    const auto inflows_path = directory / "optiflow_removed_storage_inflows.csv";
    write_series(prices_path, "price", {{"2027-01-01T00:00:00Z", 0.0}});
    write_series(inflows_path, "natural_inflow", {{"2027-01-01T00:00:00Z", 0.0}});
    {
        std::ofstream scenario(scenario_path);
        scenario << "key,value\nscenario_name,legacy\nunexpected_parameter,0\n";
    }
    require_throws<std::invalid_argument>([&] {
        static_cast<void>(core::CsvScenarioReader::read(scenario_path, prices_path, inflows_path));
    }, "unsupported scenario key");
    std::filesystem::remove(scenario_path);
    std::filesystem::remove(prices_path);
    std::filesystem::remove(inflows_path);
}

}  // namespace

int main() {
    test_transition_and_reward();
    test_zero_reward_is_canonical_positive_zero();
    test_mutual_exclusion_and_bounds();
    test_action_grid_contains_only_unique_feasible_controls();
    test_state_grid_and_linear_interpolation();
    test_scenario_validation();
    test_bellman_forward_and_runner();
    test_csv_reader_preserves_and_validates_timestamps();
    test_csv_reader_accepts_only_the_legacy_fixed_power_factor();
    test_csv_reader_rejects_unsupported_key();
    return 0;
}
