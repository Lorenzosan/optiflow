#pragma once

#include <optiflow/core/StorageTypes.hpp>

namespace optiflow {

/**
 * @brief Deterministic pumped-storage hydro transition and reward model.
 */
class PumpedStorageModel {
public:
    /**
     * @brief Construct the model and validate parameters.
     *
     * @throws std::invalid_argument if parameters are inconsistent.
     */
    explicit PumpedStorageModel(ModelParameters parameters);

    /**
     * @brief Return validated model parameters.
     */
    [[nodiscard]] const ModelParameters& parameters() const noexcept;

    /**
     * @brief Apply a hydro action to a state under one exogenous input.
     */
    [[nodiscard]] TransitionResult transition(const ReservoirState& state,
                                              const HydroAction& action,
                                              const ExogenousPoint& exogenous) const;

    /**
     * @brief Validate model parameters.
     *
     * @throws std::invalid_argument if parameters are inconsistent.
     */
    static void validate_parameters(const ModelParameters& parameters);

    /**
     * @brief Convert turbine flow to generated electrical power.
     */
    [[nodiscard]] static double turbine_power_mw(double turbine_flow_m3_s,
                                                 double hydraulic_head_m,
                                                 double turbine_efficiency);

    /**
     * @brief Convert pump flow to consumed electrical power.
     */
    [[nodiscard]] static double pump_power_mw(double pump_flow_m3_s,
                                              double hydraulic_head_m,
                                              double pump_efficiency);

private:
    ModelParameters parameters_;
};

} // namespace optiflow
