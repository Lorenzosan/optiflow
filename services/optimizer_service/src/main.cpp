#include "optiflow/demo/DemoScenario.h"
#include "optiflow/service/Http.h"

#include <cstdlib>
#include <exception>
#include <iostream>

namespace {

[[nodiscard]] auto handle_request(const optiflow::service::HttpRequest& request) -> optiflow::service::HttpResponse {
  if (request.method == "OPTIONS") {
    return optiflow::service::make_text_response({}, 204);
  }

  if (request.method == "GET" && request.path == "/health") {
    return optiflow::service::make_json_response("{\"service\":\"optimizer\",\"status\":\"ok\"}");
  }

  if (request.method == "POST" && request.path == "/v1/optimize") {
    const auto result = optiflow::demo::run_default_dispatch();
    return optiflow::service::make_json_response(optiflow::demo::simulation_to_json(result));
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
