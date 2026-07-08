#include <optiflow/solver/ForwardSimulator.hpp>

#include <stdexcept>

#include <optiflow/model/PumpedStorageModel.hpp>
#include <optiflow/numerics/ActionGrid.hpp>
#include <optiflow/solver/BellmanEvaluator.hpp>

namespace optiflow {

std::vector<DispatchStep> ForwardSimulator::simulate(const OptimizationResult& result) {
    const auto& series = result.problem.exogenous;
    if (series.empty()) {
        throw std::invalid_argument("deterministic exogenous series must not be empty");
    }
    if (result.policy.time_count() != series.size()) {
        throw std::invalid_argument("policy horizon does not match deterministic exogenous series");
    }
    if (result.value_function.time_count() != series.size() + 1) {
        throw std::invalid_argument("value-function horizon does not match deterministic exogenous series");
    }
    if (result.value_function.state_count() != result.state_grid.size()) {
        throw std::invalid_argument("value-function state count does not match state grid");
    }

    const auto parameters = to_model_parameters(result.problem);
    PumpedStorageModel model(parameters);
    const ActionGrid action_grid(ActionGridConfig{parameters.max_turbine_flow_m3_s,
                                                  parameters.max_pump_flow_m3_s,
                                                  result.problem.solver.turbine_flow_steps,
                                                  result.problem.solver.pump_flow_steps});
    std::vector<DispatchStep> steps;
    steps.reserve(series.size());

    ReservoirState state{result.problem.reservoir.initial_volume_m3};

    for (std::size_t time_index = 0; time_index < series.size(); ++time_index) {
        const auto decision = BellmanEvaluator::select_action(model,
                                                              result.state_grid,
                                                              result.value_function,
                                                              action_grid,
                                                              time_index,
                                                              state.reservoir_volume_m3,
                                                              series.points.at(time_index),
                                                              parameters.discount_factor);
        const auto& transition = decision.transition;

        steps.push_back(DispatchStep{series.points.at(time_index).time_index,
                                     series.points.at(time_index).price_eur_per_mwh,
                                     series.points.at(time_index).natural_inflow_m3_s,
                                     state.reservoir_volume_m3,
                                     transition.next_reservoir_volume_m3,
                                     decision.action,
                                     transition.turbine_power_mw,
                                     transition.pump_power_mw,
                                     transition.net_power_mw,
                                     transition.overflow_spill_m3,
                                     transition.reward_eur});

        state.reservoir_volume_m3 = transition.next_reservoir_volume_m3;
    }

    return steps;
}

} // namespace optiflow
