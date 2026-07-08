#pragma once

#include <filesystem>

#include <optiflow/core/StorageTypes.hpp>

namespace optiflow {

/**
 * @brief Read deterministic price and inflow CSV files and join them by time index.
 *
 * Expected price CSV header:
 *
 * time_index,price_eur_per_mwh
 *
 * Expected inflow CSV header:
 *
 * time_index,natural_inflow_m3_s
 *
 * @throws std::runtime_error if a file cannot be read, a row is malformed,
 * duplicate time indices are found, or the time-index sets do not match.
 */
DeterministicSeries read_deterministic_series(const std::filesystem::path& price_csv_path,
                                              const std::filesystem::path& inflow_csv_path);

} // namespace optiflow
