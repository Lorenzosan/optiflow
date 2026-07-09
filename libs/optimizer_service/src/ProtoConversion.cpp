#include "optiflow/service/ProtoConversion.h"

#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace optiflow::service {

namespace {

void require_message(bool present, std::string_view name) {
    if (!present) {
        throw std::invalid_argument("missing " + std::string(name));
    }
}

double require_double(bool present, double value, std::string_view name) {
    if (!present) {
        throw std::invalid_argument("missing " + std::string(name));
    }
    return value;
}

bool require_bool(bool present, bool value, std::string_view name) {
    if (!present) {
        throw std::invalid_argument("missing " + std::string(name));
    }
    return value;
}

std::size_t require_size(bool present, std::uint32_t value, std::string_view name) {
    if (!present) {
        throw std::invalid_argument("missing " + std::string(name));
    }
    return static_cast<std::size_t>(value);
}

std::uint32_t checked_uint32(std::size_t value, std::string_view name) {
    if (value > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::out_of_range(std::string(name) + " exceeds uint32 range");
    }
    return static_cast<std::uint32_t>(value);
}

double midpoint(double lower, double upper) {
    return 0.5 * (lower + upper);
}

std::string indexed_field(std::string_view series_name, std::size_t index, std::string_view field_name) {
    std::ostringstream stream;
    stream << series_name << '[' << index << "]." << field_name;
    return stream.str();
}

core::State convert_initial_state(const optimizer::v1::OptimizeRequest& request, bool battery_enabled) {
    require_message(request.has_initial_state(), "initial_state");

    const optimizer::v1::InitialState& initial_state = request.initial_state();
    const double reservoir_volume = require_double(initial_state.has_reservoir_volume(),
                                                   initial_state.reservoir_volume(),
                                                   "initial_state.reservoir_volume");
    const double battery_soc = battery_enabled
                                   ? require_double(initial_state.has_battery_soc(),
                                                    initial_state.battery_soc(),
                                                    "initial_state.battery_soc")
                                   : 0.0;

    return core::State(reservoir_volume, battery_soc);
}

core::ModelParameters convert_model_parameters(const optimizer::v1::OptimizeRequest& request,
                                               bool battery_enabled) {
    require_message(request.has_model(), "model");
    require_message(request.has_economics(), "economics");

    const optimizer::v1::ModelParameters& model = request.model();
    const optimizer::v1::EconomicParameters& economics = request.economics();

    const double battery_min_soc = battery_enabled
                                       ? require_double(model.has_battery_min_soc(),
                                                        model.battery_min_soc(),
                                                        "model.battery_min_soc")
                                       : 0.0;
    const double battery_max_soc = battery_enabled
                                       ? require_double(model.has_battery_max_soc(),
                                                        model.battery_max_soc(),
                                                        "model.battery_max_soc")
                                       : 0.0;
    const double battery_charge_max_power = battery_enabled
                                                ? require_double(model.has_battery_charge_max_power(),
                                                                 model.battery_charge_max_power(),
                                                                 "model.battery_charge_max_power")
                                                : 0.0;
    const double battery_discharge_max_power = battery_enabled
                                                   ? require_double(model.has_battery_discharge_max_power(),
                                                                    model.battery_discharge_max_power(),
                                                                    "model.battery_discharge_max_power")
                                                   : 0.0;
    const double battery_charge_efficiency = battery_enabled
                                                 ? require_double(model.has_battery_charge_efficiency(),
                                                                  model.battery_charge_efficiency(),
                                                                  "model.battery_charge_efficiency")
                                                 : 1.0;
    const double battery_discharge_efficiency = battery_enabled
                                                    ? require_double(model.has_battery_discharge_efficiency(),
                                                                     model.battery_discharge_efficiency(),
                                                                     "model.battery_discharge_efficiency")
                                                    : 1.0;

    return core::ModelParameters(
        require_double(model.has_time_step_hours(), model.time_step_hours(), "model.time_step_hours"),
        require_double(model.has_reservoir_min_volume(),
                       model.reservoir_min_volume(),
                       "model.reservoir_min_volume"),
        require_double(model.has_reservoir_max_volume(),
                       model.reservoir_max_volume(),
                       "model.reservoir_max_volume"),
        battery_min_soc,
        battery_max_soc,
        require_double(model.has_turbine_max_flow(), model.turbine_max_flow(), "model.turbine_max_flow"),
        require_double(model.has_pump_max_flow(), model.pump_max_flow(), "model.pump_max_flow"),
        require_double(model.has_spill_max_flow(), model.spill_max_flow(), "model.spill_max_flow"),
        battery_charge_max_power,
        battery_discharge_max_power,
        require_double(model.has_turbine_efficiency(),
                       model.turbine_efficiency(),
                       "model.turbine_efficiency"),
        require_double(model.has_pump_efficiency(), model.pump_efficiency(), "model.pump_efficiency"),
        battery_charge_efficiency,
        battery_discharge_efficiency,
        require_double(model.has_water_to_power_factor(),
                       model.water_to_power_factor(),
                       "model.water_to_power_factor"),
        require_double(economics.has_battery_degradation_cost_per_mwh(),
                       economics.battery_degradation_cost_per_mwh(),
                       "economics.battery_degradation_cost_per_mwh"),
        require_double(economics.has_operating_cost_per_mwh(),
                       economics.operating_cost_per_mwh(),
                       "economics.operating_cost_per_mwh"),
        require_double(economics.has_infeasibility_penalty(),
                       economics.infeasibility_penalty(),
                       "economics.infeasibility_penalty"));
}

core::TerminalParameters convert_terminal_parameters(const optimizer::v1::OptimizeRequest& request,
                                                     const core::ModelParameters& model_parameters,
                                                     bool battery_enabled) {
    require_message(request.has_terminal(), "terminal");

    const optimizer::v1::TerminalSettings& terminal = request.terminal();

    const bool use_hard_reservoir_bounds = require_bool(terminal.has_use_hard_reservoir_bounds(),
                                                        terminal.use_hard_reservoir_bounds(),
                                                        "terminal.use_hard_reservoir_bounds");
    const double reservoir_min_volume = use_hard_reservoir_bounds
                                            ? require_double(terminal.has_terminal_reservoir_min(),
                                                             terminal.terminal_reservoir_min(),
                                                             "terminal.terminal_reservoir_min")
                                            : model_parameters.reservoir_min_volume;
    const double reservoir_max_volume = use_hard_reservoir_bounds
                                            ? require_double(terminal.has_terminal_reservoir_max(),
                                                             terminal.terminal_reservoir_max(),
                                                             "terminal.terminal_reservoir_max")
                                            : model_parameters.reservoir_max_volume;

    const bool use_soft_reservoir_target = require_bool(terminal.has_use_soft_reservoir_target(),
                                                        terminal.use_soft_reservoir_target(),
                                                        "terminal.use_soft_reservoir_target");
    const double target_reservoir_volume = use_soft_reservoir_target
                                               ? require_double(terminal.has_terminal_reservoir_target(),
                                                                terminal.terminal_reservoir_target(),
                                                                "terminal.terminal_reservoir_target")
                                               : midpoint(reservoir_min_volume, reservoir_max_volume);
    const double reservoir_target_penalty = use_soft_reservoir_target
                                                ? require_double(terminal.has_terminal_reservoir_penalty(),
                                                                 terminal.terminal_reservoir_penalty(),
                                                                 "terminal.terminal_reservoir_penalty")
                                                : 0.0;

    double battery_min_soc = 0.0;
    double battery_max_soc = 0.0;
    double target_battery_soc = 0.0;
    double battery_target_penalty = 0.0;

    if (battery_enabled) {
        const bool use_hard_battery_bounds = require_bool(terminal.has_use_hard_battery_bounds(),
                                                          terminal.use_hard_battery_bounds(),
                                                          "terminal.use_hard_battery_bounds");
        battery_min_soc = use_hard_battery_bounds
                              ? require_double(terminal.has_terminal_battery_min(),
                                               terminal.terminal_battery_min(),
                                               "terminal.terminal_battery_min")
                              : model_parameters.battery_min_soc;
        battery_max_soc = use_hard_battery_bounds
                              ? require_double(terminal.has_terminal_battery_max(),
                                               terminal.terminal_battery_max(),
                                               "terminal.terminal_battery_max")
                              : model_parameters.battery_max_soc;

        const bool use_soft_battery_target = require_bool(terminal.has_use_soft_battery_target(),
                                                          terminal.use_soft_battery_target(),
                                                          "terminal.use_soft_battery_target");
        target_battery_soc = use_soft_battery_target
                                 ? require_double(terminal.has_terminal_battery_target(),
                                                  terminal.terminal_battery_target(),
                                                  "terminal.terminal_battery_target")
                                 : midpoint(battery_min_soc, battery_max_soc);
        battery_target_penalty = use_soft_battery_target
                                     ? require_double(terminal.has_terminal_battery_penalty(),
                                                      terminal.terminal_battery_penalty(),
                                                      "terminal.terminal_battery_penalty")
                                     : 0.0;
    }

    return core::TerminalParameters(reservoir_min_volume,
                                    reservoir_max_volume,
                                    battery_min_soc,
                                    battery_max_soc,
                                    target_reservoir_volume,
                                    target_battery_soc,
                                    reservoir_target_penalty,
                                    battery_target_penalty);
}

std::vector<core::Exogenous> convert_exogenous_series(const optimizer::v1::OptimizeRequest& request) {
    if (request.prices().empty()) {
        throw std::invalid_argument("prices must not be empty");
    }
    if (request.inflows().empty()) {
        throw std::invalid_argument("inflows must not be empty");
    }
    if (request.prices_size() != request.inflows_size()) {
        throw std::invalid_argument("prices and inflows must have matching horizons");
    }

    std::vector<core::Exogenous> series;
    series.reserve(static_cast<std::size_t>(request.prices_size()));

    for (int index = 0; index < request.prices_size(); ++index) {
        const optimizer::v1::PricePoint& price = request.prices(index);
        const optimizer::v1::InflowPoint& inflow = request.inflows(index);
        if (price.time_index() != inflow.time_index()) {
            throw std::invalid_argument(indexed_field("prices", static_cast<std::size_t>(index), "time_index") +
                                        " does not match " +
                                        indexed_field("inflows", static_cast<std::size_t>(index), "time_index"));
        }
        series.emplace_back(price.price(), inflow.natural_inflow());
    }

    return series;
}

core::SolverParameters convert_solver_parameters(const optimizer::v1::OptimizeRequest& request,
                                                 bool battery_enabled) {
    require_message(request.has_grid(), "grid");
    const optimizer::v1::GridParameters& grid = request.grid();

    const std::size_t battery_points = battery_enabled
                                           ? require_size(grid.has_battery_points(),
                                                          grid.battery_points(),
                                                          "grid.battery_points")
                                           : 1U;
    const std::size_t battery_charge_points = battery_enabled
                                                  ? require_size(grid.has_battery_charge_points(),
                                                                 grid.battery_charge_points(),
                                                                 "grid.battery_charge_points")
                                                  : 1U;
    const std::size_t battery_discharge_points = battery_enabled
                                                     ? require_size(grid.has_battery_discharge_points(),
                                                                    grid.battery_discharge_points(),
                                                                    "grid.battery_discharge_points")
                                                     : 1U;

    return core::SolverParameters(require_size(grid.has_reservoir_points(),
                                               grid.reservoir_points(),
                                               "grid.reservoir_points"),
                                  battery_points,
                                  require_size(grid.has_turbine_flow_points(),
                                               grid.turbine_flow_points(),
                                               "grid.turbine_flow_points"),
                                  require_size(grid.has_spill_flow_points(),
                                               grid.spill_flow_points(),
                                               "grid.spill_flow_points"),
                                  require_size(grid.has_pump_flow_points(),
                                               grid.pump_flow_points(),
                                               "grid.pump_flow_points"),
                                  battery_charge_points,
                                  battery_discharge_points,
                                  require_double(grid.has_discount_factor(),
                                                 grid.discount_factor(),
                                                 "grid.discount_factor"));
}

void fill_dispatch_row(const core::DispatchStep& step, optimizer::v1::DispatchRow& row) {
    row.set_time_index(checked_uint32(step.time_index, "dispatch.time_index"));
    row.set_price(step.exogenous.electricity_price);
    row.set_natural_inflow(step.exogenous.natural_inflow);
    row.set_reservoir_volume(step.state.reservoir_volume);
    row.set_battery_soc(step.state.battery_soc);
    row.set_turbine_flow(step.action.turbine_flow);
    row.set_pump_flow(step.action.pump_flow);
    row.set_spill_flow(step.action.spill_flow);
    row.set_battery_charge_power(step.action.battery_charge_power);
    row.set_battery_discharge_power(step.action.battery_discharge_power);
    row.set_net_power(step.net_power);
    row.set_reward(step.reward);
    row.set_cumulative_profit(step.cumulative_profit);
}

void fill_diagnostics(const runner::OptimizationResult& result,
                      optimizer::v1::OptimizationDiagnostics& diagnostics) {
    diagnostics.set_horizon_steps(checked_uint32(result.dispatch.size(), "diagnostics.horizon_steps"));

    std::size_t turbine_steps = 0;
    std::size_t pump_steps = 0;
    std::size_t spill_steps = 0;
    std::size_t battery_charge_steps = 0;
    std::size_t battery_discharge_steps = 0;
    std::size_t wait_steps = 0;

    for (const core::DispatchStep& step : result.dispatch) {
        const bool turbines = step.action.turbine_flow > 0.0;
        const bool pumps = step.action.pump_flow > 0.0;
        const bool spills = step.action.spill_flow > 0.0;
        const bool charges = step.action.battery_charge_power > 0.0;
        const bool discharges = step.action.battery_discharge_power > 0.0;

        turbine_steps += turbines ? 1U : 0U;
        pump_steps += pumps ? 1U : 0U;
        spill_steps += spills ? 1U : 0U;
        battery_charge_steps += charges ? 1U : 0U;
        battery_discharge_steps += discharges ? 1U : 0U;
        wait_steps += (!turbines && !pumps && !spills && !charges && !discharges) ? 1U : 0U;
    }

    diagnostics.set_turbine_steps(checked_uint32(turbine_steps, "diagnostics.turbine_steps"));
    diagnostics.set_pump_steps(checked_uint32(pump_steps, "diagnostics.pump_steps"));
    diagnostics.set_spill_steps(checked_uint32(spill_steps, "diagnostics.spill_steps"));
    diagnostics.set_battery_charge_steps(checked_uint32(battery_charge_steps,
                                                        "diagnostics.battery_charge_steps"));
    diagnostics.set_battery_discharge_steps(checked_uint32(battery_discharge_steps,
                                                           "diagnostics.battery_discharge_steps"));
    diagnostics.set_wait_steps(checked_uint32(wait_steps, "diagnostics.wait_steps"));
}

}  // namespace

core::ScenarioBundle toScenarioBundle(const optimizer::v1::OptimizeRequest& request) {
    require_message(request.has_model(), "model");
    const bool battery_enabled = require_bool(request.model().has_battery_enabled(),
                                             request.model().battery_enabled(),
                                             "model.battery_enabled");

    core::ModelParameters model_parameters = convert_model_parameters(request, battery_enabled);
    core::TerminalParameters terminal_parameters = convert_terminal_parameters(request,
                                                                              model_parameters,
                                                                              battery_enabled);
    core::Scenario scenario(request.scenario_name(),
                            convert_initial_state(request, battery_enabled),
                            convert_exogenous_series(request),
                            model_parameters,
                            terminal_parameters);

    return core::ScenarioBundle(std::move(scenario), convert_solver_parameters(request, battery_enabled));
}

void fillOptimizeResponse(const runner::OptimizationResult& result,
                          optimizer::v1::OptimizeResponse& response) {
    response.Clear();
    response.set_cumulative_profit(result.cumulative_profit);

    for (const core::DispatchStep& step : result.dispatch) {
        fill_dispatch_row(step, *response.add_dispatch());
    }

    if (!result.dispatch.empty()) {
        const core::State& final_state = result.dispatch.back().next_state;
        response.mutable_final_state()->set_reservoir_volume(final_state.reservoir_volume);
        response.mutable_final_state()->set_battery_soc(final_state.battery_soc);
    }

    fill_diagnostics(result, *response.mutable_diagnostics());
}

}  // namespace optiflow::service
