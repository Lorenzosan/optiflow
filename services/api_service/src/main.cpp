#include "optiflow/demo/DemoScenario.h"
#include "optiflow/service/Http.h"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

namespace {

std::string latest_result_json;

[[nodiscard]] auto handle_request(const optiflow::service::HttpRequest& request) -> optiflow::service::HttpResponse {
  if (request.method == "OPTIONS") {
    return optiflow::service::make_text_response({}, 204);
  }

  if (request.method == "GET" && (request.path == "/health" || request.path == "/api/health")) {
    return optiflow::service::make_json_response("{\"service\":\"api\",\"status\":\"ok\"}");
  }

  if (request.method == "GET" && request.path == "/api/scenarios/sample") {
    return optiflow::service::make_json_response(
        optiflow::demo::scenario_to_json(optiflow::demo::make_default_exogenous(),
                                         optiflow::demo::make_default_parameters()));
  }

  if (request.method == "POST" && request.path == "/api/optimizations") {
    const auto optimizer_url = optiflow::service::getenv_or(
        "OPTIFLOW_OPTIMIZER_URL",
        "http://127.0.0.1:50051/v1/optimize");

    try {
      auto response = optiflow::service::http_post(optimizer_url, request.body, "application/json");
      if (response.status_code >= 200 && response.status_code < 300) {
        latest_result_json = response.body;
      }
      return response;
    } catch (const std::exception& error) {
      const auto fallback_enabled = optiflow::service::getenv_or("OPTIFLOW_API_DIRECT_FALLBACK", "0") == "1";
      if (!fallback_enabled) {
        return optiflow::service::make_error_response(std::string{"optimizer unavailable: "} + error.what(), 502);
      }

      const auto result = optiflow::demo::run_default_dispatch();
      latest_result_json = optiflow::demo::simulation_to_json(result);
      return optiflow::service::make_json_response(latest_result_json);
    }
  }

  if (request.method == "GET" && request.path == "/api/runs/latest") {
    if (latest_result_json.empty()) {
      return optiflow::service::make_error_response("no optimization run has been created yet", 404);
    }
    return optiflow::service::make_json_response(latest_result_json);
  }

  return optiflow::service::make_error_response("route not found", 404);
}

}  // namespace

int main() {
  try {
    const auto port = optiflow::service::getenv_u16_or("OPTIFLOW_API_PORT", 8080U);
    optiflow::service::serve_forever(port, handle_request);
    return EXIT_SUCCESS;
  } catch (const std::exception& error) {
    std::cerr << "api service error: " << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
