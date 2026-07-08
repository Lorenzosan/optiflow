#include "TestSupport.hpp"

#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

#include <cmath>

namespace {

optiflow::DeterministicProblem problem() {
    optiflow::DeterministicProblem p;
    p.exogenous = optiflow::DeterministicSeries{{
        optiflow::ExogenousPoint{0, 10.0, 0.0},
        optiflow::ExogenousPoint{1, 100.0, 0.0},
    }};
    p.reservoir.min_volume_m3 = 0.0;
    p.reservoir.max_volume_m3 = 200000.0;
    p.reservoir.initial_volume_m3 = 100000.0;
    p.reservoir.max_turbine_flow_m3_s = 20.0;
    p.reservoir.max_pump_flow_m3_s = 20.0;
    p.hydro.hydraulic_head_m = 100.0;
    p.hydro.turbine_efficiency = 0.9;
    p.hydro.pump_efficiency = 0.85;
    p.hydro.timestep_hours = 1.0;
    p.economics.discount_factor = 1.0;
    p.economics.overflow_spill_penalty_eur_per_m3 = 0.0;
    p.terminal_reservoir.target_volume_m3 = p.reservoir.initial_volume_m3;
    p.terminal_reservoir.penalty_eur_per_m3 = 0.0;
    p.solver.volume_grid_points = 21;
    p.solver.turbine_flow_steps = 4;
    p.solver.pump_flow_steps = 4;
    return p;
}

} // namespace

int main() {
    return run_test([] {
        const optiflow::BellmanSolver solver;
        const auto base_problem = problem();
        const auto result = solver.solve(base_problem);
        OPTIFLOW_REQUIRE(result.objective_value_eur > 0.0);
        OPTIFLOW_REQUIRE(result.policy.time_count() == 2);
        OPTIFLOW_REQUIRE(result.value_function.time_count() == 3);
        OPTIFLOW_REQUIRE(result.problem.exogenous.size() == 2);

        auto no_turbine = problem();
        no_turbine.reservoir.max_turbine_flow_m3_s = 0.0;
        const auto no_turbine_result = solver.solve(no_turbine);
        OPTIFLOW_REQUIRE(no_turbine_result.objective_value_eur <= result.objective_value_eur);

        auto terminal_penalty = problem();
        terminal_penalty.terminal_reservoir.penalty_eur_per_m3 = 10.0;
        const auto penalized_result = solver.solve(terminal_penalty);
        const auto unpenalized_dispatch = optiflow::ForwardSimulator::simulate(result);
        const auto penalized_dispatch = optiflow::ForwardSimulator::simulate(penalized_result);
        const double unpenalized_deviation = std::abs(
            unpenalized_dispatch.back().reservoir_end_m3 - base_problem.terminal_reservoir.target_volume_m3);
        const double penalized_deviation = std::abs(
            penalized_dispatch.back().reservoir_end_m3 - terminal_penalty.terminal_reservoir.target_volume_m3);
        OPTIFLOW_REQUIRE(penalized_deviation <= unpenalized_deviation);

        auto empty_problem = problem();
        empty_problem.exogenous = optiflow::DeterministicSeries{};
        require_throws([&] { static_cast<void>(solver.solve(empty_problem)); });
    });
}
