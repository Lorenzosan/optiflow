#include "optiflow/service/ProtoConversion.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace core = optiflow::core;
namespace optimizer_v1 = optiflow::optimizer::v1;
namespace service = optiflow::service;

constexpr double tolerance = 1.0e-9;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

void require_near(double actual, double expected, std::string_view message) {
    if (std::abs(actual - expected) > tolerance) {
        throw std::runtime_error(std::string(message) + ": expected " + std::to_string(expected) +
                                 ", got " + std::to_string(actual));
    }
}

template <typename Exception, typename Function>
void require_throws(Function&& function, std::string_view message_fragment) {
    try {
        function();
    } catch (const Exception& error) {
        if (!message_fragment.empty()) {
            const std::string text(error.what());
            require(text.find(std::string(message_fragment)) != std::string::npos,
                    std::string("exception message did not contain expected text: ") + text);
        }
        return;
    } catch (const std::exception& error) {
        throw std::runtime_error(std::string("wrong exception type: ") + error.what());
    }

    throw std::runtime_error("expected exception was not thrown");
}

optimizer_v1::OptimizeRequest valid_request() {
    optimizer_v1::OptimizeRequest request;
    request.set_scenario_name("proto_smoke");

    request.mutable_initial_state()->set_reservoir_volume(50.0);
    request.mutable_initial_state()->set_battery_soc(10.0);

    optimizer_v1::ModelParameters* model = request.mutable_model();
    model->set_time_step_hours(1.0);
    model->set_reservoir_min_volume(0.0);
    model->set_reservoir_max_volume(100.0);
    model->set_turbine_max_flow(20.0);
    model->set_pump_max_flow(10.0);
    model->set_spill_max_flow(20.0);
    model->set_turbine_efficiency(0.9);
    model->set_pump_efficiency(0.85);
    model->set_battery_enabled(true);
    model->set_battery_min_soc(0.0);
    model->set_battery_max_soc(20.0);
    model->set_battery_charge_max_power(5.0);
    model->set_battery_discharge_max_power(5.0);
    model->set_battery_charge_efficiency(0.95);
    model->set_battery_discharge_efficiency(0.95);
    model->set_water_to_power_factor(0.5);

    optimizer_v1::EconomicParameters* economics = request.mutable_economics();
    economics->set_operating_cost_per_mwh(1.0);
    economics->set_battery_degradation_cost_per_mwh(2.0);
    economics->set_infeasibility_penalty(1000000.0);

    optimizer_v1::GridParameters* grid = request.mutable_grid();
    grid->set_reservoir_points(11);
    grid->set_battery_points(5);
    grid->set_turbine_flow_points(3);
    grid->set_pump_flow_points(2);
    grid->set_spill_flow_points(1);
    grid->set_battery_charge_points(2);
    grid->set_battery_discharge_points(2);
    grid->set_discount_factor(1.0);

    optimizer_v1::TerminalSettings* terminal = request.mutable_terminal();
    terminal->set_use_hard_reservoir_bounds(true);
    terminal->set_terminal_reservoir_min(0.0);
    terminal->set_terminal_reservoir_max(100.0);
    terminal->set_use_soft_reservoir_target(false);
    terminal->set_use_hard_battery_bounds(true);
    terminal->set_terminal_battery_min(0.0);
    terminal->set_terminal_battery_max(20.0);
    terminal->set_use_soft_battery_target(false);

    optimizer_v1::PricePoint* price0 = request.add_prices();
    price0->set_time_index(0);
    price0->set_price(50.0);

    optimizer_v1::PricePoint* price1 = request.add_prices();
    price1->set_time_index(1);
    price1->set_price(120.0);

    optimizer_v1::InflowPoint* inflow0 = request.add_inflows();
    inflow0->set_time_index(0);
    inflow0->set_natural_inflow(1.0);

    optimizer_v1::InflowPoint* inflow1 = request.add_inflows();
    inflow1->set_time_index(1);
    inflow1->set_natural_inflow(2.0);

    return request;
}

void test_valid_request_converts_to_scenario_bundle() {
    const core::ScenarioBundle bundle = service::toScenarioBundle(valid_request());

    require(bundle.scenario.name() == "proto_smoke", "scenario name");
    require(bundle.scenario.horizon_size() == 2, "horizon size");
    require_near(bundle.scenario.initial_state().reservoir_volume, 50.0, "initial reservoir");
    require_near(bundle.scenario.initial_state().battery_soc, 10.0, "initial battery");
    require_near(bundle.scenario.model_parameters().water_to_power_factor, 0.5, "water-to-power factor");
    require_near(bundle.scenario.model_parameters().operating_cost_per_mwh, 1.0, "operating cost");
    require(bundle.solver_parameters.reservoir_volume_grid_points == 11, "reservoir grid points");
    require(bundle.solver_parameters.battery_soc_grid_points == 5, "battery grid points");
    require(bundle.solver_parameters.pump_flow_steps == 2, "pump action points");
    require(bundle.solver_parameters.spill_flow_steps == 1, "spill action points");
    require_near(bundle.solver_parameters.discount_factor, 1.0, "discount factor");
}

void test_missing_required_field_is_rejected() {
    optimizer_v1::OptimizeRequest request = valid_request();
    request.mutable_model()->clear_reservoir_max_volume();

    require_throws<std::invalid_argument>([&]() {
        static_cast<void>(service::toScenarioBundle(request));
    }, "model.reservoir_max_volume");
}

void test_price_inflow_length_mismatch_is_rejected() {
    optimizer_v1::OptimizeRequest request = valid_request();
    request.mutable_inflows()->RemoveLast();

    require_throws<std::invalid_argument>([&]() {
        static_cast<void>(service::toScenarioBundle(request));
    }, "matching horizons");
}

void test_price_inflow_index_mismatch_is_rejected() {
    optimizer_v1::OptimizeRequest request = valid_request();
    request.mutable_inflows(1)->set_time_index(7);

    require_throws<std::invalid_argument>([&]() {
        static_cast<void>(service::toScenarioBundle(request));
    }, "prices[1].time_index");
}

void test_disabled_battery_converts_to_single_point_battery_state() {
    optimizer_v1::OptimizeRequest request = valid_request();
    request.mutable_model()->set_battery_enabled(false);
    request.mutable_model()->clear_battery_min_soc();
    request.mutable_model()->clear_battery_max_soc();
    request.mutable_model()->clear_battery_charge_max_power();
    request.mutable_model()->clear_battery_discharge_max_power();
    request.mutable_model()->clear_battery_charge_efficiency();
    request.mutable_model()->clear_battery_discharge_efficiency();
    request.mutable_initial_state()->clear_battery_soc();
    request.mutable_grid()->clear_battery_points();
    request.mutable_grid()->clear_battery_charge_points();
    request.mutable_grid()->clear_battery_discharge_points();
    request.mutable_terminal()->clear_use_hard_battery_bounds();
    request.mutable_terminal()->clear_use_soft_battery_target();
    request.mutable_terminal()->clear_terminal_battery_min();
    request.mutable_terminal()->clear_terminal_battery_max();

    const core::ScenarioBundle bundle = service::toScenarioBundle(request);

    require_near(bundle.scenario.initial_state().battery_soc, 0.0, "disabled battery initial soc");
    require_near(bundle.scenario.model_parameters().battery_max_soc, 0.0, "disabled battery max soc");
    require(bundle.solver_parameters.battery_soc_grid_points == 1, "disabled battery state points");
    require(bundle.solver_parameters.battery_charge_steps == 1, "disabled battery charge points");
    require(bundle.solver_parameters.battery_discharge_steps == 1, "disabled battery discharge points");
}

}  // namespace

int main() {
    test_valid_request_converts_to_scenario_bundle();
    test_missing_required_field_is_rejected();
    test_price_inflow_length_mismatch_is_rejected();
    test_price_inflow_index_mismatch_is_rejected();
    test_disabled_battery_converts_to_single_point_battery_state();
    return 0;
}
