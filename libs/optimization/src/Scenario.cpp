#include "optiflow/core/Scenario.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace optiflow::core {

namespace {

void require_finite(double value, const char* name) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be finite");
    }
}

}  // namespace

Scenario::Scenario(std::string name,
                   State initial_state,
                   std::vector<Exogenous> exogenous_series,
                   ModelParameters model_parameters)
    : name_(std::move(name)),
      initial_state_(initial_state),
      exogenous_series_(std::move(exogenous_series)),
      model_parameters_(model_parameters) {
    validate();
}

const std::string& Scenario::name() const {
    return name_;
}

const State& Scenario::initial_state() const {
    return initial_state_;
}

const std::vector<Exogenous>& Scenario::exogenous_series() const {
    return exogenous_series_;
}

const ModelParameters& Scenario::model_parameters() const {
    return model_parameters_;
}

std::size_t Scenario::horizon_size() const {
    return exogenous_series_.size();
}

void Scenario::validate() const {
    if (name_.empty()) {
        throw std::invalid_argument("scenario_name is required");
    }
    if (exogenous_series_.empty()) {
        throw std::invalid_argument("at least one exogenous time step is required");
    }

    validate_model_parameters(model_parameters_);

    require_finite(initial_state_.reservoir_volume, "initial_reservoir_volume");
    require_finite(initial_state_.battery_soc, "initial_battery_soc");

    if (initial_state_.reservoir_volume < model_parameters_.reservoir_min_volume ||
        initial_state_.reservoir_volume > model_parameters_.reservoir_max_volume) {
        throw std::invalid_argument("initial_reservoir_volume is outside reservoir bounds");
    }

    if (initial_state_.battery_soc < model_parameters_.battery_min_soc ||
        initial_state_.battery_soc > model_parameters_.battery_max_soc) {
        throw std::invalid_argument("initial_battery_soc is outside battery bounds");
    }

    for (const Exogenous& exogenous : exogenous_series_) {
        require_finite(exogenous.electricity_price, "electricity_price");
        require_finite(exogenous.natural_inflow, "natural_inflow");
    }
}

ScenarioBundle::ScenarioBundle(Scenario scenario_value, SolverParameters solver_parameters_value)
    : scenario(std::move(scenario_value)), solver_parameters(solver_parameters_value) {
    validate_solver_parameters(solver_parameters);
}

}  // namespace optiflow::core
