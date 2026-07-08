#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace optiflow::service {

struct HttpRequest final {
  std::string method;
  std::string path;
  std::map<std::string, std::string> headers;
  std::string body;
};

struct HttpResponse final {
  int status_code{200};
  std::string content_type{"application/json"};
  std::string body;
  std::map<std::string, std::string> headers;
};

using RequestHandler = std::function<HttpResponse(const HttpRequest&)>;

[[nodiscard]] auto make_json_response(std::string body, int status_code = 200) -> HttpResponse;
[[nodiscard]] auto make_text_response(std::string body, int status_code = 200) -> HttpResponse;
[[nodiscard]] auto make_error_response(std::string message, int status_code) -> HttpResponse;

void add_cors_headers(HttpResponse& response);
void serve_forever(std::uint16_t port, const RequestHandler& handler);

[[nodiscard]] auto http_post(std::string url, std::string body, std::string content_type) -> HttpResponse;

[[nodiscard]] auto getenv_or(std::string name, std::string fallback) -> std::string;
[[nodiscard]] auto getenv_u16_or(std::string name, std::uint16_t fallback) -> std::uint16_t;

}  // namespace optiflow::service
