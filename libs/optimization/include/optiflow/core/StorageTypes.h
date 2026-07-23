#pragma once

#include <cstddef>
#include <string>

namespace optiflow::core {

/**
 * @brief Stored hydraulic-energy inventory in the upper reservoir.
 */
struct State {
    double reservoir_volume; ///< Upper-reservoir hydraulic energy in MWh.

    explicit State(double reservoir_volume);
};

/**
 * @brief Pumped-storage control decision for one time step.
 */
struct Action {
    double turbine_flow; ///< Hydraulic power withdrawn through the turbine in MW.
    double spill_flow; ///< Hydraulic power spilled without generation in MW.
    double pump_flow; ///< Hydraulic power added to the upper reservoir in MW.

    Action(double turbine_flow, double spill_flow, double pump_flow);
};

/**
 * @brief Exogenous market and hydrological inputs for one time step.
 */
struct Exogenous {
    std::string timestamp_utc; ///< Canonical UTC interval start, empty only for programmatic tests.
    double electricity_price; ///< Electricity price in currency per MWh.
    double natural_inflow; ///< Natural inflow as hydraulic power in MW.

    Exogenous(std::string timestamp_utc, double electricity_price, double natural_inflow);
    Exogenous(double electricity_price, double natural_inflow);
};

/**
 * @brief Result of applying an action to a state.
 */
struct Outcome {
    State next_state;
    double turbine_power;
    double pump_power;
    double net_power;
    double reward;
    bool feasible;
    std::string infeasibility_reason;

    Outcome(State next_state,
            double turbine_power,
            double pump_power,
            double net_power,
            double reward,
            bool feasible,
            std::string infeasibility_reason);
};

/**
 * @brief One row of a forward-simulated dispatch trajectory.
 */
struct DispatchStep {
    std::size_t time_index;
    State state;
    Action action;
    Exogenous exogenous;
    State next_state;
    double net_power;
    double reward;

    DispatchStep(std::size_t time_index,
                 State state,
                 Action action,
                 Exogenous exogenous,
                 State next_state,
                 double net_power,
                 double reward);
};

/**
 * @brief Physical and economic pumped-storage parameters.
 */
struct ModelParameters {
    double time_step_hours;
    double reservoir_min_volume;
    double reservoir_max_volume;
    double turbine_max_flow;
    double pump_max_flow;
    double spill_max_flow;
    double turbine_efficiency;
    double pump_efficiency;
    double operating_cost_per_mwh;

    ModelParameters(double time_step_hours,
                    double reservoir_min_volume,
                    double reservoir_max_volume,
                    double turbine_max_flow,
                    double pump_max_flow,
                    double spill_max_flow,
                    double turbine_efficiency,
                    double pump_efficiency,
                    double operating_cost_per_mwh);
};

/**
 * @brief Hard and soft terminal requirements for reservoir inventory.
 */
struct TerminalParameters {
    double reservoir_min_volume;
    double reservoir_max_volume;
    double target_reservoir_volume;
    double reservoir_target_penalty;

    TerminalParameters(double reservoir_min_volume,
                       double reservoir_max_volume,
                       double target_reservoir_volume,
                       double reservoir_target_penalty);
};

/**
 * @brief Numerical resolution parameters.
 */
struct SolverParameters {
    std::size_t reservoir_volume_grid_points;
    std::size_t turbine_flow_steps;
    std::size_t spill_flow_steps;
    std::size_t pump_flow_steps;
    double discount_factor;

    SolverParameters(std::size_t reservoir_volume_grid_points,
                     std::size_t turbine_flow_steps,
                     std::size_t spill_flow_steps,
                     std::size_t pump_flow_steps,
                     double discount_factor);
};

void validate_model_parameters(const ModelParameters& parameters);
void validate_terminal_parameters(const ModelParameters& model_parameters,
                                  const TerminalParameters& terminal_parameters);
void validate_solver_parameters(const SolverParameters& parameters);

}  // namespace optiflow::core
