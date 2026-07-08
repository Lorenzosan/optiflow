#pragma once

#include <optiflow/core/StorageTypes.hpp>
#include <optiflow/solver/SolverTypes.hpp>

namespace optiflow {

/**
 * @brief Hard physical limits for the reservoir and hydro controls.
 *
 * These values describe feasibility constraints. The solver must never return
 * a policy that violates them.
 */
struct ReservoirConstraints {
    double min_volume_m3{};
    double max_volume_m3{};
    double initial_volume_m3{};

    double max_turbine_flow_m3_s{};
    double max_pump_flow_m3_s{};
};

/**
 * @brief Hydro conversion and time-step parameters.
 */
struct HydroPhysics {
    double hydraulic_head_m{};
    double turbine_efficiency{};
    double pump_efficiency{};
    double timestep_hours{1.0};
};

/**
 * @brief Economic parameters that are part of the objective function.
 */
struct EconomicParameters {
    double discount_factor{1.0};
    double overflow_spill_penalty_eur_per_m3{};
};

/**
 * @brief Soft final reservoir target.
 *
 * The Bellman terminal value is the negative penalty for ending away from this
 * target. This keeps the problem feasible when the exact target is unreachable
 * on the discretized grid.
 */
struct TerminalReservoirTarget {
    double target_volume_m3{};
    double penalty_eur_per_m3{};
};

/**
 * @brief Complete deterministic optimization problem passed to the solver.
 *
 * This is the solver boundary. CSV readers, CLIs, HTTP DTOs, databases, and
 * other transport-specific code should build this in-memory object before
 * calling BellmanSolver.
 */
struct DeterministicProblem {
    DeterministicSeries exogenous;
    ReservoirConstraints reservoir;
    HydroPhysics hydro;
    EconomicParameters economics;
    TerminalReservoirTarget terminal_reservoir;
    BellmanSolverConfig solver;
};

/**
 * @brief Convert the explicit problem representation into the flattened model parameters.
 */
[[nodiscard]] ModelParameters to_model_parameters(const DeterministicProblem& problem);

/**
 * @brief Validate the deterministic problem before solving.
 *
 * @throws std::invalid_argument if the problem is empty or inconsistent.
 */
void validate_problem(const DeterministicProblem& problem);

} // namespace optiflow
