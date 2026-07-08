#include "TestSupport.hpp"

#include <optiflow/model/PumpedStorageModel.hpp>

namespace {

optiflow::ModelParameters valid_parameters() {
    optiflow::ModelParameters parameters;
    parameters.min_reservoir_volume_m3 = 0.0;
    parameters.max_reservoir_volume_m3 = 1000.0;
    parameters.initial_reservoir_volume_m3 = 500.0;
    parameters.max_turbine_flow_m3_s = 10.0;
    parameters.max_pump_flow_m3_s = 10.0;
    parameters.hydraulic_head_m = 100.0;
    parameters.turbine_efficiency = 0.9;
    parameters.pump_efficiency = 0.8;
    parameters.timestep_hours = 1.0;
    parameters.discount_factor = 1.0;
    parameters.target_final_reservoir_volume_m3 = 500.0;
    parameters.terminal_reservoir_penalty_eur_per_m3 = 0.01;
    parameters.overflow_spill_penalty_eur_per_m3 = 0.02;
    return parameters;
}

} // namespace

int main() {
    return run_test([] {
        const auto parameters = valid_parameters();
        const optiflow::PumpedStorageModel model(parameters);

        const double turbine_power = optiflow::PumpedStorageModel::turbine_power_mw(1.0, 100.0, 0.9);
        const double pump_power = optiflow::PumpedStorageModel::pump_power_mw(1.0, 100.0, 0.8);
        OPTIFLOW_REQUIRE(turbine_power > 0.0);
        OPTIFLOW_REQUIRE(pump_power > 0.0);

        const optiflow::ReservoirState state{500.0};
        const optiflow::HydroAction idle{optiflow::HydroMode::Idle, 0.0, 0.0};
        const optiflow::ExogenousPoint exogenous{0, 50.0, 0.1};
        const auto transition = model.transition(state, idle, exogenous);
        OPTIFLOW_REQUIRE(transition.feasible);
        OPTIFLOW_REQUIRE_NEAR(transition.next_reservoir_volume_m3, 860.0, 1.0e-12);

        const optiflow::HydroAction invalid{optiflow::HydroMode::Turbine, 1.0, 1.0};
        OPTIFLOW_REQUIRE(!model.transition(state, invalid, exogenous).feasible);

        auto bad = valid_parameters();
        bad.initial_reservoir_volume_m3 = 2000.0;
        require_throws([&] { optiflow::PumpedStorageModel invalid_model(bad); });

        auto bad_target = valid_parameters();
        bad_target.target_final_reservoir_volume_m3 = 2000.0;
        require_throws([&] { optiflow::PumpedStorageModel invalid_model(bad_target); });

        auto bad_penalty = valid_parameters();
        bad_penalty.terminal_reservoir_penalty_eur_per_m3 = -1.0;
        require_throws([&] { optiflow::PumpedStorageModel invalid_model(bad_penalty); });
    });
}
