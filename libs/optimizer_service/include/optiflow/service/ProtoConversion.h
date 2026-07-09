#pragma once

#include "optiflow/core/Scenario.h"
#include "optiflow/runner/OptimizationRunner.h"

#include "optiflow/optimizer/v1/optimizer.pb.h"

namespace optiflow::service {

/**
 * @brief Convert a protobuf optimization request to the core scenario bundle.
 *
 * @param request Protobuf request received at the service boundary.
 * @return Core scenario bundle suitable for OptimizationRunner.
 * @throws std::invalid_argument if required fields are absent or inconsistent.
 */
core::ScenarioBundle toScenarioBundle(const optimizer::v1::OptimizeRequest& request);

/**
 * @brief Fill a protobuf response from a core optimization result.
 *
 * @param result Core optimization result.
 * @param response Response message to overwrite.
 */
void fillOptimizeResponse(const runner::OptimizationResult& result,
                          optimizer::v1::OptimizeResponse& response);

}  // namespace optiflow::service
