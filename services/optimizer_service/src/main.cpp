#include "optiflow/demo/DemoScenario.h"
#include "optiflow/demo/OptimizationRequest.h"
#include "optiflow/service/Http.h"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

[[nodiscard]] auto handle_request(const optiflow::service::HttpRequest& request) -> optiflow::service::HttpResponse {
  if (request.method == "GET" && request.path == "/health") {
    return optiflow::service::make_json_response("{\"service\":\"optimizer\",\"status\":\"ok\"}");
  }

  if (request.method == "POST" && request.path == "/v1/optimize") {
    try {
      const auto optimization_request = optiflow::demo::parse_optimization_request_json(request.body);
      const auto result = optimization_request.solver_kind == optiflow::demo::RequestSolverKind::Stochastic
          ? optiflow::demo::run_stochastic_dispatch(
                optimization_request.stochastic_process,
                optimization_request.parameters,
                optimization_request.initial_state,
                optimization_request.config)
          : optiflow::demo::run_dispatch(
                optimization_request.exogenous,
                optimization_request.parameters,
                optimization_request.initial_state,
                optimization_request.config);
      return optiflow::service::make_json_response(optiflow::demo::simulation_to_json(result));
    } catch (const std::invalid_argument& error) {
      return optiflow::service::make_error_response(error.what(), 400);
    }
  }

  return optiflow::service::make_error_response("route not found", 404);
}

}  // namespace

int main() {
  try {
    const auto port = optiflow::service::getenv_u16_or("OPTIFLOW_OPTIMIZER_PORT", 50051U);
    optiflow::service::serve_forever(port, handle_request);
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "optimizer service error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
