#pragma once

#include <cstddef>
#include <string>

namespace optiflow::core {

/**
 * @brief Stored water inventory in the upper reservoir.
 */
struct State {
    double reservoir_volume; ///< Upper-reservoir volume.

    explicit State(double reservoir_volume);
};

/**
 * @brief Pumped-storage control decision for one time step.
 */
struct Action {
    double turbine_flow; ///< Water released through the turbine.
    double spill_flow; ///< Water released without generation.
    double pump_flow; ///< Water pumped into the upper reservoir.

    Action(double turbine_flow, double spill_flow, double pump_flow);
};

/**
 * @brief Exogenous market and hydrological inputs for one time step.
 */
struct Exogenous {
    double electricity_price; ///< Electricity price in currency per MWh.
    double natural_inflow; ///< Natural inflow in reservoir-volume units per hour.

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
    double cumulative_profit;

    DispatchStep(std::size_t time_index,
                 State state,
                 Action action,
                 Exogenous exogenous,
                 State next_state,
                 double net_power,
                 double reward,
                 double cumulative_profit);
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
    double water_to_power_factor;
    double operating_cost_per_mwh;
    double infeasibility_penalty;

    ModelParameters(double time_step_hours,
                    double reservoir_min_volume,
                    double reservoir_max_volume,
                    double turbine_max_flow,
                    double pump_max_flow,
                    double spill_max_flow,
                    double turbine_efficiency,
                    double pump_efficiency,
                    double water_to_power_factor,
                    double operating_cost_per_mwh,
                    double infeasibility_penalty);
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
