#include "TestSupport.hpp"

#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

int main() {
    return run_test([] {
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
        p.terminal_water_value_eur_per_m3 = 0.0;
        p.overflow_spill_penalty_eur_per_m3 = 0.0;

        const optiflow::DeterministicSeries series{{
            optiflow::ExogenousPoint{0, 10.0, 0.0},
            optiflow::ExogenousPoint{1, 100.0, 0.0},
            optiflow::ExogenousPoint{2, 20.0, 0.0},
        }};

        const optiflow::BellmanSolver solver(optiflow::BellmanSolverConfig{21, 4, 4});
        const auto result = solver.solve(series, p);
        const auto dispatch = optiflow::ForwardSimulator::simulate(result, series);

        OPTIFLOW_REQUIRE(dispatch.size() == series.size());
        for (const auto& step : dispatch) {
            OPTIFLOW_REQUIRE(step.reservoir_end_m3 >= p.min_reservoir_volume_m3);
            OPTIFLOW_REQUIRE(step.reservoir_end_m3 <= p.max_reservoir_volume_m3);
            OPTIFLOW_REQUIRE(!(step.action.turbine_flow_m3_s > 0.0 && step.action.pump_flow_m3_s > 0.0));
        }
    });
}
