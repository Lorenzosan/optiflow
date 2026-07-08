#include "TestSupport.hpp"

#include <optiflow/solver/BellmanSolver.hpp>
#include <optiflow/solver/ForwardSimulator.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace {

constexpr double tolerance = 1.0e-6;

optiflow::DeterministicProblem reference_problem() {
    optiflow::DeterministicProblem problem;
    problem.exogenous = optiflow::DeterministicSeries{{
        optiflow::ExogenousPoint{0, 42.0, 18.0},
        optiflow::ExogenousPoint{1, 38.0, 17.0},
        optiflow::ExogenousPoint{2, 35.0, 16.0},
        optiflow::ExogenousPoint{3, 32.0, 15.0},
        optiflow::ExogenousPoint{4, 30.0, 15.0},
        optiflow::ExogenousPoint{5, 34.0, 14.0},
        optiflow::ExogenousPoint{6, 45.0, 13.0},
        optiflow::ExogenousPoint{7, 70.0, 12.0},
        optiflow::ExogenousPoint{8, 85.0, 12.0},
        optiflow::ExogenousPoint{9, 90.0, 13.0},
        optiflow::ExogenousPoint{10, 75.0, 14.0},
        optiflow::ExogenousPoint{11, 55.0, 16.0},
        optiflow::ExogenousPoint{12, 45.0, 18.0},
        optiflow::ExogenousPoint{13, 40.0, 20.0},
        optiflow::ExogenousPoint{14, 38.0, 22.0},
        optiflow::ExogenousPoint{15, 35.0, 24.0},
        optiflow::ExogenousPoint{16, 32.0, 24.0},
        optiflow::ExogenousPoint{17, 30.0, 22.0},
        optiflow::ExogenousPoint{18, 50.0, 20.0},
        optiflow::ExogenousPoint{19, 95.0, 18.0},
        optiflow::ExogenousPoint{20, 110.0, 17.0},
        optiflow::ExogenousPoint{21, 100.0, 16.0},
        optiflow::ExogenousPoint{22, 70.0, 16.0},
        optiflow::ExogenousPoint{23, 50.0, 17.0},
    }};

    problem.reservoir.min_volume_m3 = 100000.0;
    problem.reservoir.max_volume_m3 = 1000000.0;
    problem.reservoir.initial_volume_m3 = 500000.0;
    problem.reservoir.max_turbine_flow_m3_s = 80.0;
    problem.reservoir.max_pump_flow_m3_s = 60.0;

    problem.hydro.hydraulic_head_m = 300.0;
    problem.hydro.turbine_efficiency = 0.9;
    problem.hydro.pump_efficiency = 0.85;
    problem.hydro.timestep_hours = 1.0;

    problem.economics.discount_factor = 1.0;
    problem.economics.overflow_spill_penalty_eur_per_m3 = 0.01;

    problem.terminal_reservoir.target_volume_m3 = 500000.0;
    problem.terminal_reservoir.penalty_eur_per_m3 = 0.10;

    problem.solver.volume_grid_points = 101;
    problem.solver.turbine_flow_steps = 8;
    problem.solver.pump_flow_steps = 6;

    return problem;
}

double terminal_penalty(const std::vector<optiflow::DispatchStep>& dispatch,
                        const optiflow::DeterministicProblem& problem) {
    const double final_reservoir = dispatch.empty()
                                       ? problem.reservoir.initial_volume_m3
                                       : dispatch.back().reservoir_end_m3;
    return std::abs(final_reservoir - problem.terminal_reservoir.target_volume_m3) *
           problem.terminal_reservoir.penalty_eur_per_m3;
}

double total_reward(const std::vector<optiflow::DispatchStep>& dispatch) {
    double reward = 0.0;
    for (const auto& step : dispatch) {
        reward += step.reward_eur;
    }
    return reward;
}

double forward_total_value(const std::vector<optiflow::DispatchStep>& dispatch,
                           const optiflow::DeterministicProblem& problem) {
    return total_reward(dispatch) - terminal_penalty(dispatch, problem);
}

double final_reservoir_deviation(const std::vector<optiflow::DispatchStep>& dispatch,
                                 const optiflow::DeterministicProblem& problem) {
    const double final_reservoir = dispatch.empty()
                                       ? problem.reservoir.initial_volume_m3
                                       : dispatch.back().reservoir_end_m3;
    return std::abs(final_reservoir - problem.terminal_reservoir.target_volume_m3);
}

bool grid_contains(const optiflow::StateGrid& grid, double value) {
    for (const double grid_value : grid.values()) {
        if (std::abs(grid_value - value) <= tolerance) {
            return true;
        }
    }
    return false;
}

void require_physical_rollout(const std::vector<optiflow::DispatchStep>& dispatch,
                              const optiflow::DeterministicProblem& problem) {
    OPTIFLOW_REQUIRE(dispatch.size() == problem.exogenous.size());

    const double timestep_seconds = problem.hydro.timestep_hours * 3600.0;
    for (const auto& step : dispatch) {
        OPTIFLOW_REQUIRE(step.reservoir_start_m3 >= problem.reservoir.min_volume_m3 - tolerance);
        OPTIFLOW_REQUIRE(step.reservoir_start_m3 <= problem.reservoir.max_volume_m3 + tolerance);
        OPTIFLOW_REQUIRE(step.reservoir_end_m3 >= problem.reservoir.min_volume_m3 - tolerance);
        OPTIFLOW_REQUIRE(step.reservoir_end_m3 <= problem.reservoir.max_volume_m3 + tolerance);
        OPTIFLOW_REQUIRE(step.overflow_spill_m3 >= -tolerance);
        OPTIFLOW_REQUIRE(!(step.action.turbine_flow_m3_s > tolerance &&
                           step.action.pump_flow_m3_s > tolerance));

        const double expected_end = step.reservoir_start_m3 +
                                    timestep_seconds * step.natural_inflow_m3_s -
                                    timestep_seconds * step.action.turbine_flow_m3_s +
                                    timestep_seconds * step.action.pump_flow_m3_s -
                                    step.overflow_spill_m3;
        OPTIFLOW_REQUIRE_NEAR(step.reservoir_end_m3, expected_end, tolerance);
    }
}

void require_no_turbining(const std::vector<optiflow::DispatchStep>& dispatch) {
    for (const auto& step : dispatch) {
        OPTIFLOW_REQUIRE_NEAR(step.action.turbine_flow_m3_s, 0.0, tolerance);
        OPTIFLOW_REQUIRE_NEAR(step.turbine_power_mw, 0.0, tolerance);
    }
}

void require_no_pumping(const std::vector<optiflow::DispatchStep>& dispatch) {
    for (const auto& step : dispatch) {
        OPTIFLOW_REQUIRE_NEAR(step.action.pump_flow_m3_s, 0.0, tolerance);
        OPTIFLOW_REQUIRE_NEAR(step.pump_power_mw, 0.0, tolerance);
    }
}

} // namespace

int main() {
    return run_test([] {
        const optiflow::BellmanSolver solver;
        const auto problem = reference_problem();
        const auto result = solver.solve(problem);
        const auto dispatch = optiflow::ForwardSimulator::simulate(result);

        require_physical_rollout(dispatch, problem);
        OPTIFLOW_REQUIRE(grid_contains(result.state_grid, problem.reservoir.initial_volume_m3));
        OPTIFLOW_REQUIRE(grid_contains(result.state_grid, problem.terminal_reservoir.target_volume_m3));
        OPTIFLOW_REQUIRE_NEAR(forward_total_value(dispatch, problem), result.objective_value_eur, tolerance);
        OPTIFLOW_REQUIRE_NEAR(terminal_penalty(dispatch, problem), 360.0, tolerance);

        auto no_turbine_problem = reference_problem();
        no_turbine_problem.reservoir.max_turbine_flow_m3_s = 0.0;
        const auto no_turbine_result = solver.solve(no_turbine_problem);
        const auto no_turbine_dispatch = optiflow::ForwardSimulator::simulate(no_turbine_result);
        require_physical_rollout(no_turbine_dispatch, no_turbine_problem);
        require_no_turbining(no_turbine_dispatch);
        OPTIFLOW_REQUIRE(no_turbine_result.objective_value_eur <= result.objective_value_eur + tolerance);

        auto no_pump_problem = reference_problem();
        no_pump_problem.reservoir.max_pump_flow_m3_s = 0.0;
        const auto no_pump_result = solver.solve(no_pump_problem);
        const auto no_pump_dispatch = optiflow::ForwardSimulator::simulate(no_pump_result);
        require_physical_rollout(no_pump_dispatch, no_pump_problem);
        require_no_pumping(no_pump_dispatch);

        double previous_objective = std::numeric_limits<double>::infinity();
        double low_penalty_deviation = 0.0;
        double high_penalty_deviation = 0.0;
        for (const double penalty : {0.0, 0.01, 0.05, 0.10, 0.50, 1.00, 5.00}) {
            auto sweep_problem = reference_problem();
            sweep_problem.terminal_reservoir.penalty_eur_per_m3 = penalty;
            const auto sweep_result = solver.solve(sweep_problem);
            const auto sweep_dispatch = optiflow::ForwardSimulator::simulate(sweep_result);
            require_physical_rollout(sweep_dispatch, sweep_problem);
            OPTIFLOW_REQUIRE_NEAR(forward_total_value(sweep_dispatch, sweep_problem),
                                  sweep_result.objective_value_eur,
                                  tolerance);
            OPTIFLOW_REQUIRE(sweep_result.objective_value_eur <= previous_objective + tolerance);
            previous_objective = sweep_result.objective_value_eur;

            if (std::abs(penalty - 0.0) <= tolerance) {
                low_penalty_deviation = final_reservoir_deviation(sweep_dispatch, sweep_problem);
            }
            if (std::abs(penalty - 5.0) <= tolerance) {
                high_penalty_deviation = final_reservoir_deviation(sweep_dispatch, sweep_problem);
            }
        }
        OPTIFLOW_REQUIRE(high_penalty_deviation <= low_penalty_deviation);
    });
}
