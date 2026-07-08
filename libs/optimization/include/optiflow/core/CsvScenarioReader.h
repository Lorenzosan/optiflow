#pragma once

#include "optiflow/core/Scenario.h"

#include <filesystem>

namespace optiflow::core {

/**
 * @brief Reader for CSV scenario and constraint files.
 */
class CsvScenarioReader {
public:
    /**
     * @brief Load a scenario bundle from CSV files.
     *
     * @param timeseries_path CSV file with time_index, price, and natural_inflow columns.
     * @param constraints_path CSV file with key and value columns.
     * @return Scenario and solver parameters.
     */
    static ScenarioBundle read(const std::filesystem::path& timeseries_path,
                               const std::filesystem::path& constraints_path);
};

}  // namespace optiflow::core
