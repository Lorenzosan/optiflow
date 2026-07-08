#include <optiflow/solver/ForwardSimulator.hpp>

#include <stdexcept>

#include <optiflow/model/PumpedStorageModel.hpp>

namespace optiflow {

std::vector<DispatchStep> ForwardSimulator::simulate(const OptimizationResult& result) {
    const auto& series = result.problem.exogenous;
    if (series.empty()) {
        throw std::invalid_argument("deterministic exogenous series must not be empty");
    }
    if (result.policy.time_count() != series.size()) {
        throw std::invalid_argument("policy horizon does not match deterministic exogenous series");
    }

    const auto parameters = to_model_parameters(result.problem);
    PumpedStorageModel model(parameters);
    std::vector<DispatchStep> steps;
    steps.reserve(series.size());

    ReservoirState state{result.problem.reservoir.initial_volume_m3};

    for (std::size_t time_index = 0; time_index < series.size(); ++time_index) {
        const auto state_index = result.state_grid.nearest_index(state.reservoir_volume_m3);
        const auto action = result.policy.at(time_index, state_index);
        const auto transition = model.transition(state, action, series.points.at(time_index));
        if (!transition.feasible) {
            throw std::runtime_error("policy produced an infeasible forward transition");
        }

        steps.push_back(DispatchStep{series.points.at(time_index).time_index,
                                     series.points.at(time_index).price_eur_per_mwh,
                                     series.points.at(time_index).natural_inflow_m3_s,
                                     state.reservoir_volume_m3,
                                     transition.next_reservoir_volume_m3,
                                     action,
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
