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
                                 0.9, 0.85, 0.5, 1.0);
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
    near(outcome.turbine_power, 4.5, "turbine power");
    near(outcome.net_power, 4.5, "net power");
    near(outcome.reward, 445.5, "reward");
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
        1.0, 0.0, 100.0, 20.0, 0.0, 0.0, 0.9, 0.85, 0.5, 1.0);
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

void write_series(const std::filesystem::path& path, const char* header) {
    std::ofstream output(path);
    output << header << "\n0,0\n";
}

void test_csv_reader_rejects_unsupported_key() {
    const auto directory = std::filesystem::temp_directory_path();
    const auto scenario_path = directory / "optiflow_removed_storage.csv";
    const auto prices_path = directory / "optiflow_removed_storage_prices.csv";
    const auto inflows_path = directory / "optiflow_removed_storage_inflows.csv";
    write_series(prices_path, "time_index,price");
    write_series(inflows_path, "time_index,natural_inflow");
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
    test_mutual_exclusion_and_bounds();
    test_action_grid_contains_only_unique_feasible_controls();
    test_state_grid_and_linear_interpolation();
    test_scenario_validation();
    test_bellman_forward_and_runner();
    test_csv_reader_rejects_unsupported_key();
    return 0;
}
