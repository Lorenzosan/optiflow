#include "optiflow/core/Scenario.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace optiflow::core {

Scenario::Scenario(std::string name,
                   State initial_state,
                   std::vector<Exogenous> exogenous_series,
                   ModelParameters model_parameters,
                   TerminalParameters terminal_parameters)
    : name_(std::move(name)),
      initial_state_(initial_state),
      exogenous_series_(std::move(exogenous_series)),
      model_parameters_(model_parameters),
      terminal_parameters_(terminal_parameters) {
    validate();
}

const std::string& Scenario::name() const { return name_; }
const State& Scenario::initial_state() const { return initial_state_; }
const std::vector<Exogenous>& Scenario::exogenous_series() const { return exogenous_series_; }
const ModelParameters& Scenario::model_parameters() const { return model_parameters_; }
const TerminalParameters& Scenario::terminal_parameters() const { return terminal_parameters_; }
std::size_t Scenario::horizon_size() const { return exogenous_series_.size(); }

void Scenario::validate() const {
    if (name_.empty()) {
        throw std::invalid_argument("scenario_name is required");
    }
    if (exogenous_series_.empty()) {
        throw std::invalid_argument("at least one exogenous time step is required");
    }

    validate_model_parameters(model_parameters_);
    validate_terminal_parameters(model_parameters_, terminal_parameters_);

    if (!std::isfinite(initial_state_.reservoir_volume)) {
        throw std::invalid_argument("initial_reservoir_volume must be finite");
    }
    if (initial_state_.reservoir_volume < model_parameters_.reservoir_min_volume ||
        initial_state_.reservoir_volume > model_parameters_.reservoir_max_volume) {
        throw std::invalid_argument("initial_reservoir_volume is outside reservoir bounds");
    }

    for (const Exogenous& exogenous : exogenous_series_) {
        if (!std::isfinite(exogenous.electricity_price)) {
            throw std::invalid_argument("electricity_price must be finite");
        }
        if (!std::isfinite(exogenous.natural_inflow)) {
            throw std::invalid_argument("natural_inflow must be finite");
        }
        if (exogenous.natural_inflow < 0.0) {
            throw std::invalid_argument("natural_inflow must be nonnegative");
        }
    }
}

ScenarioBundle::ScenarioBundle(Scenario scenario_value, SolverParameters solver_parameters_value)
    : scenario(std::move(scenario_value)), solver_parameters(solver_parameters_value) {
    validate_solver_parameters(solver_parameters);
    const ModelParameters& model_parameters = scenario.model_parameters();
    if (model_parameters.reservoir_min_volume < model_parameters.reservoir_max_volume &&
        solver_parameters.reservoir_volume_grid_points < 2) {
        throw std::invalid_argument(
            "reservoir_volume_grid_points must be at least two when reservoir bounds have nonzero width");
    }
}

}  // namespace optiflow::core
