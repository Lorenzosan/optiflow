#include "TestSupport.hpp"

#include <optiflow/solver/DeterministicProblem.hpp>

int main() {
    return run_test([] {
        optiflow::DeterministicProblem problem;
        problem.exogenous = optiflow::DeterministicSeries{{
            optiflow::ExogenousPoint{0, 50.0, 1.0},
        }};
        problem.reservoir.min_volume_m3 = 0.0;
        problem.reservoir.max_volume_m3 = 1000.0;
        problem.reservoir.initial_volume_m3 = 500.0;
        problem.reservoir.max_turbine_flow_m3_s = 10.0;
        problem.reservoir.max_pump_flow_m3_s = 10.0;
        problem.hydro.hydraulic_head_m = 100.0;
        problem.hydro.turbine_efficiency = 0.9;
        problem.hydro.pump_efficiency = 0.85;
        problem.hydro.timestep_hours = 1.0;
        problem.economics.discount_factor = 1.0;
        problem.economics.overflow_spill_penalty_eur_per_m3 = 0.01;
        problem.terminal_reservoir.target_volume_m3 = 500.0;
        problem.terminal_reservoir.penalty_eur_per_m3 = 0.10;
        problem.solver.volume_grid_points = 11;
        problem.solver.turbine_flow_steps = 2;
        problem.solver.pump_flow_steps = 2;

        optiflow::validate_problem(problem);

        const auto parameters = optiflow::to_model_parameters(problem);
        OPTIFLOW_REQUIRE_NEAR(parameters.initial_reservoir_volume_m3,
                              problem.reservoir.initial_volume_m3,
                              1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(parameters.target_final_reservoir_volume_m3,
                              problem.terminal_reservoir.target_volume_m3,
                              1.0e-12);
        OPTIFLOW_REQUIRE_NEAR(parameters.terminal_reservoir_penalty_eur_per_m3,
                              problem.terminal_reservoir.penalty_eur_per_m3,
                              1.0e-12);

        auto empty = problem;
        empty.exogenous = optiflow::DeterministicSeries{};
        require_throws([&] { optiflow::validate_problem(empty); });

        auto bad_grid = problem;
        bad_grid.solver.volume_grid_points = 1;
        require_throws([&] { optiflow::validate_problem(bad_grid); });
    });
}
