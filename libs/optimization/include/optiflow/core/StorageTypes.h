#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace optiflow::core {

/**
 * @brief Physical storage state at one time step.
 */
struct State {
    double reservoir_volume; ///< Reservoir volume.
    double battery_soc; ///< Battery state of charge.

    /**
     * @brief Construct a physical storage state.
     *
     * @param reservoir_volume Reservoir volume.
     * @param battery_soc Battery state of charge.
     */
    State(double reservoir_volume, double battery_soc);
};

/**
 * @brief Control decision applied during one time step.
 */
struct Action {
    double turbine_flow; ///< Turbine flow.
    double spill_flow; ///< Spill flow.
    double pump_flow; ///< Pump flow.
    double battery_charge_power; ///< Battery charging power.
    double battery_discharge_power; ///< Battery discharging power.

    /**
     * @brief Construct a control action.
     *
     * @param turbine_flow Turbine flow.
     * @param spill_flow Spill flow.
     * @param pump_flow Pump flow.
     * @param battery_charge_power Battery charging power.
     * @param battery_discharge_power Battery discharging power.
     */
    Action(double turbine_flow,
           double spill_flow,
           double pump_flow,
           double battery_charge_power,
           double battery_discharge_power);
};

/**
 * @brief Exogenous inputs for one time step.
 */
struct Exogenous {
    double electricity_price; ///< Electricity price.
    double natural_inflow; ///< Natural inflow into the upper reservoir.

    /**
     * @brief Construct exogenous inputs.
     *
     * @param electricity_price Electricity price.
     * @param natural_inflow Natural inflow into the upper reservoir.
     */
    Exogenous(double electricity_price, double natural_inflow);
};

/**
 * @brief Result of applying an action to a state.
 */
struct Outcome {
    State next_state; ///< State after applying the action.
    double turbine_power; ///< Electrical turbine power.
    double pump_power; ///< Electrical pumping power consumption.
    double net_power; ///< Net exported power.
    double reward; ///< One-step economic reward.
    bool feasible; ///< Whether all constraints are satisfied.
    std::string infeasibility_reason; ///< Human-readable reason for infeasibility.

    /**
     * @brief Construct a model outcome.
     *
     * @param next_state State after the action.
     * @param turbine_power Electrical turbine power.
     * @param pump_power Electrical pumping power consumption.
     * @param net_power Net exported power.
     * @param reward One-step reward.
     * @param feasible Whether all constraints are satisfied.
     * @param infeasibility_reason Human-readable infeasibility reason.
     */
    Outcome(State next_state,
            double turbine_power,
            double pump_power,
            double net_power,
            double reward,
            bool feasible,
            std::string infeasibility_reason);
};

/**
 * @brief Dispatch record produced by forward simulation.
 */
struct DispatchStep {
    std::size_t time_index; ///< Time index.
    State state; ///< State before dispatch.
    Action action; ///< Applied action.
    Exogenous exogenous; ///< Exogenous inputs.
    State next_state; ///< State after dispatch.
    double net_power; ///< Net exported power.
    double reward; ///< One-step reward.
    double cumulative_profit; ///< Profit accumulated up to this step.

    /**
     * @brief Construct one dispatch trajectory row.
     *
     * @param time_index Time index.
     * @param state State before dispatch.
     * @param action Applied action.
     * @param exogenous Exogenous inputs.
     * @param next_state State after dispatch.
     * @param net_power Net exported power.
     * @param reward One-step reward.
     * @param cumulative_profit Profit accumulated up to this step.
     */
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
 * @brief Physical and economic model parameters.
 */
struct ModelParameters {
    double time_step_hours; ///< Duration of one time step in hours.
    double reservoir_min_volume; ///< Minimum reservoir volume.
    double reservoir_max_volume; ///< Maximum reservoir volume.
    double battery_min_soc; ///< Minimum battery state of charge.
    double battery_max_soc; ///< Maximum battery state of charge.
    double turbine_max_flow; ///< Maximum turbine flow.
    double pump_max_flow; ///< Maximum pump flow.
    double spill_max_flow; ///< Maximum spill flow.
    double battery_max_charge_power; ///< Maximum battery charging power.
    double battery_max_discharge_power; ///< Maximum battery discharging power.
    double turbine_efficiency; ///< Turbine conversion efficiency.
    double pump_efficiency; ///< Pump conversion efficiency.
    double battery_charge_efficiency; ///< Battery charging efficiency.
    double battery_discharge_efficiency; ///< Battery discharging efficiency.
    double water_to_power_factor; ///< Conversion factor from water flow to electrical power.
    double battery_degradation_cost_per_mwh; ///< Battery degradation cost per MWh of throughput.
    double operating_cost_per_mwh; ///< Operating cost per MWh of throughput.
    double infeasibility_penalty; ///< Penalty assigned to infeasible transitions.

    /**
     * @brief Construct physical and economic model parameters.
     */
    ModelParameters(double time_step_hours,
                    double reservoir_min_volume,
                    double reservoir_max_volume,
                    double battery_min_soc,
                    double battery_max_soc,
                    double turbine_max_flow,
                    double pump_max_flow,
                    double spill_max_flow,
                    double battery_max_charge_power,
                    double battery_max_discharge_power,
                    double turbine_efficiency,
                    double pump_efficiency,
                    double battery_charge_efficiency,
                    double battery_discharge_efficiency,
                    double water_to_power_factor,
                    double battery_degradation_cost_per_mwh,
                    double operating_cost_per_mwh,
                    double infeasibility_penalty);
};


/**
 * @brief Terminal-state requirements and penalties.
 */
struct TerminalParameters {
    double reservoir_min_volume; ///< Minimum allowed final reservoir volume.
    double reservoir_max_volume; ///< Maximum allowed final reservoir volume.
    double battery_min_soc; ///< Minimum allowed final battery state of charge.
    double battery_max_soc; ///< Maximum allowed final battery state of charge.
    double target_reservoir_volume; ///< Preferred final reservoir volume.
    double target_battery_soc; ///< Preferred final battery state of charge.
    double reservoir_target_penalty; ///< Penalty coefficient for squared reservoir target deviation.
    double battery_target_penalty; ///< Penalty coefficient for squared battery target deviation.

    /**
     * @brief Construct terminal-state requirements and penalties.
     */
    TerminalParameters(double reservoir_min_volume,
                       double reservoir_max_volume,
                       double battery_min_soc,
                       double battery_max_soc,
                       double target_reservoir_volume,
                       double target_battery_soc,
                       double reservoir_target_penalty,
                       double battery_target_penalty);
};

/**
 * @brief Numerical parameters used by grids and the Bellman solver.
 */
struct SolverParameters {
    std::size_t reservoir_volume_grid_points; ///< Number of reservoir-volume grid points.
    std::size_t battery_soc_grid_points; ///< Number of battery-SOC grid points.
    std::size_t turbine_flow_steps; ///< Number of turbine-flow action candidates.
    std::size_t spill_flow_steps; ///< Number of spill-flow action candidates.
    std::size_t pump_flow_steps; ///< Number of pump-flow action candidates.
    std::size_t battery_charge_steps; ///< Number of battery-charge action candidates.
    std::size_t battery_discharge_steps; ///< Number of battery-discharge action candidates.
    double discount_factor; ///< Bellman discount factor.

    /**
     * @brief Construct numerical solver parameters.
     */
    SolverParameters(std::size_t reservoir_volume_grid_points,
                     std::size_t battery_soc_grid_points,
                     std::size_t turbine_flow_steps,
                     std::size_t spill_flow_steps,
                     std::size_t pump_flow_steps,
                     std::size_t battery_charge_steps,
                     std::size_t battery_discharge_steps,
                     double discount_factor);
};

/**
 * @brief Validate physical and economic model parameters.
 *
 * @param parameters Parameters to validate.
 * @throws std::invalid_argument if a value is inconsistent.
 */
void validate_model_parameters(const ModelParameters& parameters);

/**
 * @brief Validate terminal-state requirements.
 *
 * @param model_parameters Physical and economic model parameters.
 * @param terminal_parameters Terminal-state requirements and penalties.
 * @throws std::invalid_argument if a value is inconsistent.
 */
void validate_terminal_parameters(const ModelParameters& model_parameters,
                                  const TerminalParameters& terminal_parameters);

/**
 * @brief Validate numerical solver parameters.
 *
 * @param parameters Parameters to validate.
 * @throws std::invalid_argument if a value is inconsistent.
 */
void validate_solver_parameters(const SolverParameters& parameters);

}  // namespace optiflow::core
