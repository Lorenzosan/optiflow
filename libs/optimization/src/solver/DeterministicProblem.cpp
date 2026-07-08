#include <optiflow/solver/DeterministicProblem.hpp>

#include <stdexcept>

#include <optiflow/model/PumpedStorageModel.hpp>

namespace optiflow {

ModelParameters to_model_parameters(const DeterministicProblem& problem) {
    ModelParameters parameters;
    parameters.min_reservoir_volume_m3 = problem.reservoir.min_volume_m3;
    parameters.max_reservoir_volume_m3 = problem.reservoir.max_volume_m3;
    parameters.initial_reservoir_volume_m3 = problem.reservoir.initial_volume_m3;
    parameters.max_turbine_flow_m3_s = problem.reservoir.max_turbine_flow_m3_s;
    parameters.max_pump_flow_m3_s = problem.reservoir.max_pump_flow_m3_s;
    parameters.hydraulic_head_m = problem.hydro.hydraulic_head_m;
    parameters.turbine_efficiency = problem.hydro.turbine_efficiency;
    parameters.pump_efficiency = problem.hydro.pump_efficiency;
    parameters.timestep_hours = problem.hydro.timestep_hours;
    parameters.discount_factor = problem.economics.discount_factor;
    parameters.target_final_reservoir_volume_m3 = problem.terminal_reservoir.target_volume_m3;
    parameters.terminal_reservoir_penalty_eur_per_m3 =
        problem.terminal_reservoir.penalty_eur_per_m3;
    parameters.overflow_spill_penalty_eur_per_m3 =
        problem.economics.overflow_spill_penalty_eur_per_m3;
    return parameters;
}

void validate_problem(const DeterministicProblem& problem) {
    if (problem.exogenous.empty()) {
        throw std::invalid_argument("deterministic exogenous series must not be empty");
    }
    if (problem.solver.volume_grid_points < 2) {
        throw std::invalid_argument("volume grid must contain at least two points");
    }

    PumpedStorageModel::validate_parameters(to_model_parameters(problem));
}

} // namespace optiflow
