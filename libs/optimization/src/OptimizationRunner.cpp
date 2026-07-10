#include "optiflow/runner/OptimizationRunner.h"

#include "optiflow/model/PumpedStorageModel.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/BellmanSolver.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <chrono>
#include <utility>
#include <vector>

namespace optiflow::runner {

namespace {

OptimizationDiagnostics dispatch_diagnostics(const std::vector<core::DispatchStep>& dispatch,
                                             double time_step_hours,
                                             const core::State& initial_state) {
    OptimizationDiagnostics diagnostics{};
    diagnostics.horizon_steps = dispatch.size();
    diagnostics.final_reservoir_volume = initial_state.reservoir_volume;
    diagnostics.final_battery_soc = initial_state.battery_soc;

    for (const core::DispatchStep& step : dispatch) {
        const bool turbines = step.action.turbine_flow > 0.0;
        const bool pumps = step.action.pump_flow > 0.0;
        const bool spills = step.action.spill_flow > 0.0;
        const bool charges = step.action.battery_charge_power > 0.0;
        const bool discharges = step.action.battery_discharge_power > 0.0;

        diagnostics.turbine_steps += turbines ? 1U : 0U;
        diagnostics.pump_steps += pumps ? 1U : 0U;
        diagnostics.spill_steps += spills ? 1U : 0U;
        diagnostics.battery_charge_steps += charges ? 1U : 0U;
        diagnostics.battery_discharge_steps += discharges ? 1U : 0U;
        diagnostics.wait_steps += (!turbines && !pumps && !spills && !charges && !discharges) ? 1U : 0U;
        diagnostics.export_energy_mwh += step.net_power > 0.0 ? step.net_power * time_step_hours : 0.0;
        diagnostics.import_energy_mwh += step.net_power < 0.0 ? -step.net_power * time_step_hours : 0.0;
        diagnostics.final_reservoir_volume = step.next_state.reservoir_volume;
        diagnostics.final_battery_soc = step.next_state.battery_soc;
    }

    return diagnostics;
}

}  // namespace

OptimizationResult OptimizationRunner::run(const core::ScenarioBundle& bundle) const {
    using Clock = std::chrono::steady_clock;

    const core::Scenario& scenario = bundle.scenario;
    const core::ModelParameters& model_parameters = scenario.model_parameters();
    const core::SolverParameters& solver_parameters = bundle.solver_parameters;

    const numerics::StateGrid state_grid =
        numerics::StateGrid::from_parameters(model_parameters, solver_parameters);

    const numerics::ActionGrid action_grid =
        numerics::ActionGrid::from_parameters(model_parameters, solver_parameters);

    const model::PumpedStorageModel model(model_parameters);

    const solver::BellmanSolver bellman_solver(
        state_grid,
        action_grid,
        model,
        solver_parameters);

    const Clock::time_point solve_start = Clock::now();
    const solver::BellmanResult solution = bellman_solver.solve(scenario);
    const Clock::time_point solve_end = Clock::now();

    const solver::ForwardSimulator simulator(
        state_grid,
        action_grid,
        model,
        solver_parameters);

    const Clock::time_point simulation_start = Clock::now();
    std::vector<core::DispatchStep> dispatch =
        simulator.simulate_from_value_function(scenario, solution.value_function);
    const Clock::time_point simulation_end = Clock::now();

    const double cumulative_profit =
        dispatch.empty() ? 0.0 : dispatch.back().cumulative_profit;

    OptimizationDiagnostics diagnostics = dispatch_diagnostics(
        dispatch, model_parameters.time_step_hours, scenario.initial_state());
    diagnostics.horizon_steps = scenario.horizon_size();
    diagnostics.reservoir_grid_points = state_grid.reservoir_size();
    diagnostics.battery_grid_points = state_grid.battery_size();
    diagnostics.action_count = action_grid.size();
    diagnostics.solve_seconds = std::chrono::duration<double>(solve_end - solve_start).count();
    diagnostics.simulation_seconds = std::chrono::duration<double>(simulation_end - simulation_start).count();

    return OptimizationResult{std::move(dispatch), cumulative_profit, diagnostics};
}

}  // namespace optiflow::runner
