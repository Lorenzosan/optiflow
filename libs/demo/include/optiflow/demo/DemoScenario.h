#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/numerics/ActionGrid.h"
#include "optiflow/numerics/StateGrid.h"
#include "optiflow/solver/ForwardSimulator.h"

#include <string>
#include <vector>

namespace optiflow::demo {

[[nodiscard]] auto make_default_parameters() -> ModelParameters;
[[nodiscard]] auto make_default_exogenous() -> std::vector<Exogenous>;
[[nodiscard]] auto make_default_state_grid() -> StateGrid;
[[nodiscard]] auto make_default_action_grid() -> ActionGrid;
[[nodiscard]] auto make_default_initial_state() -> State;

[[nodiscard]] auto run_dispatch(const std::vector<Exogenous>& exogenous) -> SimulationResult;
[[nodiscard]] auto run_default_dispatch() -> SimulationResult;

[[nodiscard]] auto scenario_to_json(const std::vector<Exogenous>& exogenous,
                                    const ModelParameters& parameters) -> std::string;
[[nodiscard]] auto simulation_to_json(const SimulationResult& result) -> std::string;
[[nodiscard]] auto simulation_to_csv(const SimulationResult& result) -> std::string;

}  // namespace optiflow::demo
