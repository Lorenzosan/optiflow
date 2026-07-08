#pragma once

#include "optiflow/core/StorageTypes.h"

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

}  // namespace optiflow::demo
