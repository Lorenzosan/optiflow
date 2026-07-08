#pragma once

#include "optiflow/core/StorageTypes.h"

namespace optiflow::model {

/**
 * @brief Deterministic transition and reward model for pumped storage with an optional battery.
 */
class PumpedStorageModel {
public:
    /**
     * @brief Construct the physical model.
     *
     * @param parameters Physical and economic model parameters.
     */
    explicit PumpedStorageModel(core::ModelParameters parameters);

    /**
     * @brief Apply one action to one state under one exogenous input.
     *
     * @param state State before dispatch.
     * @param action Control action.
     * @param exogenous Price and inflow input.
     * @return Transition outcome.
     */
    core::Outcome apply(core::State state,
                        core::Action action,
                        core::Exogenous exogenous) const;

    /**
     * @brief Return model parameters.
     *
     * @return Physical and economic model parameters.
     */
    const core::ModelParameters& parameters() const;

private:
    core::ModelParameters parameters_;

    /**
     * @brief Build an infeasible transition outcome.
     */
    core::Outcome infeasible(core::State state, const char* reason) const;
};

}  // namespace optiflow::model
