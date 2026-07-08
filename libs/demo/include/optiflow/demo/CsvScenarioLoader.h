#pragma once

#include "optiflow/core/StorageTypes.h"
#include "optiflow/stochastic/StochasticTypes.h"

#include <string>
#include <vector>

namespace optiflow::demo {

/**
 * Loads deterministic exogenous inputs from separate price and inflow CSV files.
 *
 * The price file must contain: time_index,price_eur_per_mwh
 * The inflow file must contain: time_index,natural_inflow_m3_s
 * The time index column may also be named hour.
 */
[[nodiscard]] auto load_deterministic_exogenous_csv(const std::string& price_csv_path,
                                                    const std::string& inflow_csv_path)
    -> std::vector<Exogenous>;

/**
 * Loads a stochastic stagewise exogenous process from separate price and inflow CSV files.
 *
 * The price file must contain: time_index,realization_index,probability,price_eur_per_mwh
 * The inflow file must contain: time_index,realization_index,probability,natural_inflow_m3_s
 *
 * The loader joins rows by (time_index, realization_index). The probability columns must match
 * between the two files. This preserves correlation between price and inflow realizations. It does
 * not form a Cartesian product of independent marginal distributions.
 */
[[nodiscard]] auto load_stochastic_exogenous_csv(const std::string& price_csv_path,
                                                 const std::string& inflow_csv_path)
    -> StochasticExogenousProcess;

}  // namespace optiflow::demo
