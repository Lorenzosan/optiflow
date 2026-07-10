#pragma once

#include "optiflow/core/StorageTypes.h"

namespace optiflow::model {

/** @brief Deterministic transition and reward model for pumped-storage hydro. */
class PumpedStorageModel {
public:
    explicit PumpedStorageModel(core::ModelParameters parameters);

    core::Outcome apply(core::State state,
                        core::Action action,
                        core::Exogenous exogenous) const;

    const core::ModelParameters& parameters() const;

private:
    core::ModelParameters parameters_;
    core::Outcome infeasible(core::State state, const char* reason) const;
};

}  // namespace optiflow::model
