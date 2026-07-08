#pragma once

#include "optiflow/core/Scenario.h"

#include <filesystem>

namespace optiflow::core {

/**
 * @brief Reader for CSV scenario, price, and inflow files.
 */
class CsvScenarioReader {
public:
    /**
     * @brief Load a scenario bundle from separate CSV input files.
     *
     * @param scenario_path CSV file with key,value rows for scenario, model, terminal, and solver parameters.
     * @param prices_path CSV file with time_index and price columns.
     * @param inflows_path CSV file with time_index and natural_inflow columns.
     * @return Scenario and solver parameters.
     */
    static ScenarioBundle read(const std::filesystem::path& scenario_path,
                               const std::filesystem::path& prices_path,
                               const std::filesystem::path& inflows_path);
};

}  // namespace optiflow::core
