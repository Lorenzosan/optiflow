#include "TestSupport.hpp"

#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

int main() {
    return run_test([] {
        optiflow::DeterministicProblem problem;
        problem.exogenous = optiflow::DeterministicSeries{{
            optiflow::ExogenousPoint{0, 10.0, 0.0},
            optiflow::ExogenousPoint{1, 100.0, 0.0},
            optiflow::ExogenousPoint{2, 20.0, 0.0},
        }};
        problem.reservoir.min_volume_m3 = 0.0;
        problem.reservoir.max_volume_m3 = 200000.0;
        problem.reservoir.initial_volume_m3 = 100000.0;
        problem.reservoir.max_turbine_flow_m3_s = 20.0;
        problem.reservoir.max_pump_flow_m3_s = 20.0;
        problem.hydro.hydraulic_head_m = 100.0;
        problem.hydro.turbine_efficiency = 0.9;
        problem.hydro.pump_efficiency = 0.85;
        problem.hydro.timestep_hours = 1.0;
        problem.economics.discount_factor = 1.0;
        problem.economics.overflow_spill_penalty_eur_per_m3 = 0.0;
        problem.terminal_reservoir.target_volume_m3 = problem.reservoir.initial_volume_m3;
        problem.terminal_reservoir.penalty_eur_per_m3 = 0.0;
        problem.solver.volume_grid_points = 21;
        problem.solver.turbine_flow_steps = 4;
        problem.solver.pump_flow_steps = 4;

        const optiflow::BellmanSolver solver;
        const auto result = solver.solve(problem);
        const auto dispatch = optiflow::ForwardSimulator::simulate(result);

        OPTIFLOW_REQUIRE(dispatch.size() == problem.exogenous.size());
        for (const auto& step : dispatch) {
            OPTIFLOW_REQUIRE(step.reservoir_end_m3 >= problem.reservoir.min_volume_m3);
            OPTIFLOW_REQUIRE(step.reservoir_end_m3 <= problem.reservoir.max_volume_m3);
            OPTIFLOW_REQUIRE(!(step.action.turbine_flow_m3_s > 0.0 && step.action.pump_flow_m3_s > 0.0));
        }
    });
}
