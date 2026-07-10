#pragma once

#include "optiflow/core/Scenario.h"

#include <filesystem>

namespace optiflow::core {

class CsvScenarioReader {
public:
    static ScenarioBundle read(const std::filesystem::path& scenario_path,
                               const std::filesystem::path& prices_path,
                               const std::filesystem::path& inflows_path);
};

}  // namespace optiflow::core
