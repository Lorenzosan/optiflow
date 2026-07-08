#pragma once

#include <cstddef>
#include <vector>

#include <optiflow/core/StorageEnums.hpp>

namespace optiflow {

/**
 * @brief Deterministic exogenous input at one time step.
 */
struct ExogenousPoint {
    std::size_t time_index{};
    double price_eur_per_mwh{};
    double natural_inflow_m3_s{};
};

/**
 * @brief Deterministic exogenous time series used by the optimizer.
 */
struct DeterministicSeries {
    std::vector<ExogenousPoint> points;

    /**
     * @brief Return the number of time steps in the series.
     */
    [[nodiscard]] std::size_t size() const noexcept;

    /**
     * @brief Return true when the series contains no time steps.
     */
    [[nodiscard]] bool empty() const noexcept;
};

/**
 * @brief Reservoir-only physical state.
 */
struct ReservoirState {
    double reservoir_volume_m3{};
};

/**
 * @brief Hydro action in natural water-flow units.
 *
 * Only one of turbine flow and pump flow is expected to be positive. Use
 * HydroMode to describe the intended operating mode.
 */
struct HydroAction {
    HydroMode mode{HydroMode::Idle};
    double turbine_flow_m3_s{};
    double pump_flow_m3_s{};
};

/**
 * @brief Physical and economic parameters for the deterministic hydro model.
 */
struct ModelParameters {
    double min_reservoir_volume_m3{};
    double max_reservoir_volume_m3{};
    double initial_reservoir_volume_m3{};

    double max_turbine_flow_m3_s{};
    double max_pump_flow_m3_s{};

    double hydraulic_head_m{};
    double turbine_efficiency{};
    double pump_efficiency{};

    double timestep_hours{1.0};
    double discount_factor{1.0};

    double terminal_water_value_eur_per_m3{};
    double overflow_spill_penalty_eur_per_m3{};
};

/**
 * @brief Result of applying one hydro action to one model state.
 */
struct TransitionResult {
    bool feasible{};
    double next_reservoir_volume_m3{};
    double overflow_spill_m3{};
    double turbine_power_mw{};
    double pump_power_mw{};
    double net_power_mw{};
    double market_revenue_eur{};
    double overflow_spill_penalty_eur{};
    double reward_eur{};
};

} // namespace optiflow
