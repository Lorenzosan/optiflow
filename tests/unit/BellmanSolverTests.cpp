#include "TestSupport.hpp"

#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

#include <cmath>

namespace {

optiflow::ModelParameters parameters() {
    optiflow::ModelParameters p;
    p.min_reservoir_volume_m3 = 0.0;
    p.max_reservoir_volume_m3 = 200000.0;
    p.initial_reservoir_volume_m3 = 100000.0;
    p.max_turbine_flow_m3_s = 20.0;
    p.max_pump_flow_m3_s = 20.0;
    p.hydraulic_head_m = 100.0;
    p.turbine_efficiency = 0.9;
    p.pump_efficiency = 0.85;
    p.timestep_hours = 1.0;
    p.discount_factor = 1.0;
    p.target_final_reservoir_volume_m3 = p.initial_reservoir_volume_m3;
    p.terminal_reservoir_penalty_eur_per_m3 = 0.0;
    p.overflow_spill_penalty_eur_per_m3 = 0.0;
    return p;
}

optiflow::DeterministicSeries series() {
    return optiflow::DeterministicSeries{{
        optiflow::ExogenousPoint{0, 10.0, 0.0},
        optiflow::ExogenousPoint{1, 100.0, 0.0},
    }};
}

} // namespace

int main() {
    return run_test([] {
        const optiflow::BellmanSolver solver(optiflow::BellmanSolverConfig{21, 4, 4});
        const auto result = solver.solve(series(), parameters());
        OPTIFLOW_REQUIRE(result.objective_value_eur > 0.0);
        OPTIFLOW_REQUIRE(result.policy.time_count() == 2);
        OPTIFLOW_REQUIRE(result.value_function.time_count() == 3);

        auto no_turbine = parameters();
        no_turbine.max_turbine_flow_m3_s = 0.0;
        const auto no_turbine_result = solver.solve(series(), no_turbine);
        OPTIFLOW_REQUIRE(no_turbine_result.objective_value_eur <= result.objective_value_eur);

        auto terminal_penalty = parameters();
        terminal_penalty.terminal_reservoir_penalty_eur_per_m3 = 10.0;
        const auto penalized_result = solver.solve(series(), terminal_penalty);
        const auto unpenalized_dispatch = optiflow::ForwardSimulator::simulate(result, series());
        const auto penalized_dispatch = optiflow::ForwardSimulator::simulate(penalized_result, series());
        const double unpenalized_deviation = std::abs(unpenalized_dispatch.back().reservoir_end_m3 -
                                                      parameters().target_final_reservoir_volume_m3);
        const double penalized_deviation = std::abs(penalized_dispatch.back().reservoir_end_m3 -
                                                    terminal_penalty.target_final_reservoir_volume_m3);
        OPTIFLOW_REQUIRE(penalized_deviation <= unpenalized_deviation);

        require_throws([&] { static_cast<void>(solver.solve(optiflow::DeterministicSeries{}, parameters())); });
    });
}
