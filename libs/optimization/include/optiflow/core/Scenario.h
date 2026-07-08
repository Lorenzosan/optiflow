#pragma once

#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace optiflow::core {

/**
 * @brief Complete input scenario for one optimization run.
 */
class Scenario {
public:
    /**
     * @brief Construct a scenario.
     *
     * @param name Scenario name.
     * @param initial_state Initial physical state.
     * @param exogenous_series Price and inflow series.
     * @param model_parameters Physical and economic model parameters.
     * @param terminal_parameters Terminal-state requirements and penalties.
     */
    Scenario(std::string name,
             State initial_state,
             std::vector<Exogenous> exogenous_series,
             ModelParameters model_parameters,
             TerminalParameters terminal_parameters);

    /**
     * @brief Return the scenario name.
     *
     * @return Scenario name.
     */
    const std::string& name() const;

    /**
     * @brief Return the initial state.
     *
     * @return Initial physical state.
     */
    const State& initial_state() const;

    /**
     * @brief Return the exogenous input series.
     *
     * @return Price and inflow series.
     */
    const std::vector<Exogenous>& exogenous_series() const;

    /**
     * @brief Return the model parameters.
     *
     * @return Physical and economic model parameters.
     */
    const ModelParameters& model_parameters() const;

    /**
     * @brief Return terminal-state requirements and penalties.
     *
     * @return Terminal-state parameters.
     */
    const TerminalParameters& terminal_parameters() const;

    /**
     * @brief Return the finite-horizon length.
     *
     * @return Number of optimization time steps.
     */
    std::size_t horizon_size() const;

private:
    std::string name_;
    State initial_state_;
    std::vector<Exogenous> exogenous_series_;
    ModelParameters model_parameters_;
    TerminalParameters terminal_parameters_;

    /**
     * @brief Validate internal consistency.
     *
     * @throws std::invalid_argument if required inputs are missing or inconsistent.
     */
    void validate() const;
};

/**
 * @brief Scenario and numerical parameters loaded from files.
 */
struct ScenarioBundle {
    Scenario scenario; ///< Scenario input.
    SolverParameters solver_parameters; ///< Numerical solver parameters.

    /**
     * @brief Construct a scenario bundle.
     *
     * @param scenario Scenario input.
     * @param solver_parameters Numerical solver parameters.
     */
    ScenarioBundle(Scenario scenario, SolverParameters solver_parameters);
};

}  // namespace optiflow::core
