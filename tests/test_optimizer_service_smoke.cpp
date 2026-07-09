#include "optiflow/service/OptimizerServiceImpl.h"

#include "optiflow/optimizer/v1/optimizer.grpc.pb.h"

#include <grpcpp/grpcpp.h>

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace optimizer_v1 = optiflow::optimizer::v1;
namespace service = optiflow::service;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

optimizer_v1::OptimizeRequest valid_request() {
    optimizer_v1::OptimizeRequest request;
    request.set_scenario_name("grpc_service_smoke");

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

void test_valid_optimize_request_returns_dispatch() {
    service::OptimizerServiceImpl service_impl;

    grpc::ServerContext context;
    optimizer_v1::OptimizeRequest request = valid_request();
    optimizer_v1::OptimizeResponse response;

    const grpc::Status status = service_impl.Optimize(&context, &request, &response);

    require(status.ok(), "Optimize service call failed: " + status.error_message());
    require(response.dispatch_size() == 2, "dispatch row count should match horizon");
    require(response.has_final_state(), "response should include final state");
    require(std::isfinite(response.cumulative_profit()), "cumulative profit should be finite");
    require(std::isfinite(response.final_state().reservoir_volume()), "final reservoir should be finite");
    require(std::isfinite(response.final_state().battery_soc()), "final battery state should be finite");

    std::size_t turbine_steps = 0;
    std::size_t pump_steps = 0;
    std::size_t spill_steps = 0;
    std::size_t battery_charge_steps = 0;
    std::size_t battery_discharge_steps = 0;
    std::size_t wait_steps = 0;

    for (const optimizer_v1::DispatchRow& row : response.dispatch()) {
        const bool turbines = row.turbine_flow() > 0.0;
        const bool pumps = row.pump_flow() > 0.0;
        const bool spills = row.spill_flow() > 0.0;
        const bool charges = row.battery_charge_power() > 0.0;
        const bool discharges = row.battery_discharge_power() > 0.0;

        turbine_steps += turbines ? 1U : 0U;
        pump_steps += pumps ? 1U : 0U;
        spill_steps += spills ? 1U : 0U;
        battery_charge_steps += charges ? 1U : 0U;
        battery_discharge_steps += discharges ? 1U : 0U;
        wait_steps += (!turbines && !pumps && !spills && !charges && !discharges) ? 1U : 0U;
    }

    const optimizer_v1::OptimizationDiagnostics& diagnostics = response.diagnostics();
    require(diagnostics.horizon_steps() == response.dispatch_size(),
            "diagnostic horizon should match dispatch size");
    require(diagnostics.reservoir_grid_points() == 11, "diagnostic reservoir grid points");
    require(diagnostics.battery_grid_points() == 5, "diagnostic battery grid points");
    require(diagnostics.action_count() == 24, "diagnostic action count");
    require(std::isfinite(diagnostics.solve_seconds()) && diagnostics.solve_seconds() >= 0.0,
            "diagnostic solve time should be finite and nonnegative");
    require(std::isfinite(diagnostics.simulation_seconds()) && diagnostics.simulation_seconds() >= 0.0,
            "diagnostic simulation time should be finite and nonnegative");
    require(diagnostics.turbine_steps() == turbine_steps, "diagnostic turbine steps");
    require(diagnostics.pump_steps() == pump_steps, "diagnostic pump steps");
    require(diagnostics.spill_steps() == spill_steps, "diagnostic spill steps");
    require(diagnostics.battery_charge_steps() == battery_charge_steps, "diagnostic battery charge steps");
    require(diagnostics.battery_discharge_steps() == battery_discharge_steps,
            "diagnostic battery discharge steps");
    require(diagnostics.wait_steps() == wait_steps, "diagnostic wait steps");
}

void test_invalid_request_returns_invalid_argument() {
    service::OptimizerServiceImpl service_impl;

    optimizer_v1::OptimizeRequest request = valid_request();
    request.mutable_model()->clear_reservoir_max_volume();

    grpc::ServerContext context;
    optimizer_v1::OptimizeResponse response;

    const grpc::Status status = service_impl.Optimize(&context, &request, &response);

    require(!status.ok(), "invalid Optimize request should fail");
    require(status.error_code() == grpc::StatusCode::INVALID_ARGUMENT,
            "invalid Optimize request should return INVALID_ARGUMENT");
    require(status.error_message().find("model.reservoir_max_volume") != std::string::npos,
            "invalid Optimize request should report missing reservoir bound");
}

}  // namespace

int main() {
    test_valid_optimize_request_returns_dispatch();
    test_invalid_request_returns_invalid_argument();
    return 0;
}
