#pragma once

#include "optiflow/core/StorageTypes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace optiflow::core {

class Scenario {
public:
    Scenario(std::string name,
             State initial_state,
             std::vector<Exogenous> exogenous_series,
             ModelParameters model_parameters,
             TerminalParameters terminal_parameters);

    const std::string& name() const;
    const State& initial_state() const;
    const std::vector<Exogenous>& exogenous_series() const;
    const ModelParameters& model_parameters() const;
    const TerminalParameters& terminal_parameters() const;
    std::size_t horizon_size() const;

private:
    std::string name_;
    State initial_state_;
    std::vector<Exogenous> exogenous_series_;
    ModelParameters model_parameters_;
    TerminalParameters terminal_parameters_;

    void validate() const;
};

struct ScenarioBundle {
    Scenario scenario;
    SolverParameters solver_parameters;

    ScenarioBundle(Scenario scenario, SolverParameters solver_parameters);
};

}  // namespace optiflow::core
