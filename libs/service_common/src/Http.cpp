#include "optiflow/service/Http.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>

namespace optiflow::service {
namespace {

[[nodiscard]] auto trim(std::string value) -> std::string {
  const auto is_space = [](const unsigned char character) { return character == ' ' || character == '\t' || character == '\r' || character == '\n'; };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](const char character) {
                return !is_space(static_cast<unsigned char>(character));
              }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [&](const char character) {
                return !is_space(static_cast<unsigned char>(character));
              }).base(),
              value.end());
  return value;
}

[[nodiscard]] auto lower_copy(std::string value) -> std::string {
  std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  return value;
}

[[nodiscard]] auto reason_phrase(const int status_code) -> std::string_view {
  switch (status_code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 204:
      return "No Content";
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 502:
      return "Bad Gateway";
    case 500:
      return "Internal Server Error";
    default:
      return "Unknown";
  }
}

[[nodiscard]] auto parse_content_length(const std::map<std::string, std::string>& headers) -> std::size_t {
  const auto iterator = headers.find("content-length");
  if (iterator == headers.end()) {
    return 0U;
  }

  std::size_t length{};
  const auto text = iterator->second;
  const auto* begin = text.data();
  const auto* end = text.data() + text.size();
  const auto [ptr, error] = std::from_chars(begin, end, length);
  if (error != std::errc{} || ptr != end) {
    throw std::runtime_error{"invalid content-length header"};
  }
  return length;
}

[[nodiscard]] auto parse_request(const std::string& raw) -> HttpRequest {
  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    throw std::runtime_error{"incomplete HTTP request"};
  }

  std::istringstream stream{raw.substr(0U, header_end)};
  HttpRequest request;
  std::string version;
  stream >> request.method >> request.path >> version;
  if (request.method.empty() || request.path.empty()) {
    throw std::runtime_error{"invalid HTTP request line"};
  }

  std::string line;
  std::getline(stream, line);
  while (std::getline(stream, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    const auto name = lower_copy(trim(line.substr(0U, separator)));
    const auto value = trim(line.substr(separator + 1U));
    request.headers.emplace(name, value);
  }

  request.body = raw.substr(header_end + 4U);
  return request;
}

[[nodiscard]] auto serialize_response(const HttpResponse& response) -> std::string {
  std::ostringstream stream;
  stream << "HTTP/1.1 " << response.status_code << ' ' << reason_phrase(response.status_code) << "\r\n";
  stream << "Content-Type: " << response.content_type << "\r\n";
  stream << "Content-Length: " << response.body.size() << "\r\n";
  stream << "Connection: close\r\n";
  for (const auto& [name, value] : response.headers) {
    const auto normalized_name = lower_copy(name);
    if (normalized_name == "content-type" || normalized_name == "content-length" || normalized_name == "connection") {
      continue;
    }
    stream << name << ": " << value << "\r\n";
  }
  stream << "\r\n" << response.body;
  return stream.str();
}

void close_socket(const int socket_fd) noexcept {
  if (socket_fd >= 0) {
    static_cast<void>(::close(socket_fd));
  }
}

[[nodiscard]] auto read_http_message(const int socket_fd) -> std::string {
  std::string raw;
  std::array<char, 4096U> buffer{};

  while (raw.find("\r\n\r\n") == std::string::npos) {
    const auto bytes_read = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
    if (bytes_read <= 0) {
      break;
    }
    raw.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    if (raw.size() > 1'000'000U) {
      throw std::runtime_error{"HTTP headers too large"};
    }
  }

  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    return raw;
  }

  const auto request = parse_request(raw);
  const auto content_length = parse_content_length(request.headers);
  const auto body_start = header_end + 4U;
  while (raw.size() < body_start + content_length) {
    const auto bytes_read = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
    if (bytes_read <= 0) {
      break;
    }
    raw.append(buffer.data(), static_cast<std::size_t>(bytes_read));
  }

  return raw;
}

struct ParsedUrl final {
  std::string host;
  std::string port;
  std::string path;
};

[[nodiscard]] auto parse_http_url(const std::string& url) -> ParsedUrl {
  constexpr std::string_view prefix{"http://"};
  if (url.rfind(prefix, 0U) != 0U) {
    throw std::invalid_argument{"only http:// URLs are supported by the lightweight client"};
  }

  const auto without_scheme = url.substr(prefix.size());
  const auto path_pos = without_scheme.find('/');
  const auto authority = path_pos == std::string::npos ? without_scheme : without_scheme.substr(0U, path_pos);
  const auto path = path_pos == std::string::npos ? std::string{"/"} : without_scheme.substr(path_pos);
  const auto colon_pos = authority.find(':');
  if (authority.empty()) {
    throw std::invalid_argument{"URL host is empty"};
  }

  if (colon_pos == std::string::npos) {
    return ParsedUrl{.host = authority, .port = "80", .path = path};
  }

  return ParsedUrl{.host = authority.substr(0U, colon_pos), .port = authority.substr(colon_pos + 1U), .path = path};
}

[[nodiscard]] auto connect_to_host(const std::string& host, const std::string& port) -> int {
  addrinfo hints{};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  addrinfo* results = nullptr;
  const auto status = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &results);
  if (status != 0) {
    throw std::runtime_error{std::string{"getaddrinfo failed: "} + ::gai_strerror(status)};
  }

  for (auto* candidate = results; candidate != nullptr; candidate = candidate->ai_next) {
    const auto socket_fd = ::socket(candidate->ai_family, candidate->ai_socktype, candidate->ai_protocol);
    if (socket_fd < 0) {
      continue;
    }
    if (::connect(socket_fd, candidate->ai_addr, candidate->ai_addrlen) == 0) {
      ::freeaddrinfo(results);
      return socket_fd;
    }
    close_socket(socket_fd);
  }

  ::freeaddrinfo(results);
  throw std::runtime_error{"could not connect to " + host + ':' + port};
}

[[nodiscard]] auto parse_response(const std::string& raw) -> HttpResponse {
  const auto header_end = raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    throw std::runtime_error{"incomplete HTTP response"};
  }

  std::istringstream stream{raw.substr(0U, header_end)};
  std::string version;
  HttpResponse response;
  stream >> version >> response.status_code;

  std::string line;
  std::getline(stream, line);
  while (std::getline(stream, line)) {
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    const auto name = lower_copy(trim(line.substr(0U, separator)));
    const auto value = trim(line.substr(separator + 1U));
    if (name == "content-type") {
      response.content_type = value;
    } else if (name != "content-length" && name != "connection") {
      response.headers.emplace(name, value);
    }
  }

  response.body = raw.substr(header_end + 4U);
  return response;
}

void send_all(const int socket_fd, const std::string& payload) {
  std::size_t total_sent = 0U;
  while (total_sent < payload.size()) {
    const auto sent = ::send(socket_fd, payload.data() + total_sent, payload.size() - total_sent, 0);
    if (sent < 0) {
      throw std::runtime_error{std::string{"send failed: "} + std::strerror(errno)};
    }
    total_sent += static_cast<std::size_t>(sent);
  }
}

}  // namespace

[[nodiscard]] auto make_json_response(std::string body, const int status_code) -> HttpResponse {
  return HttpResponse{.status_code = status_code, .content_type = "application/json", .body = std::move(body), .headers = {}};
}

[[nodiscard]] auto make_text_response(std::string body, const int status_code) -> HttpResponse {
  return HttpResponse{.status_code = status_code, .content_type = "text/plain; charset=utf-8", .body = std::move(body), .headers = {}};
}

[[nodiscard]] auto make_error_response(std::string message, const int status_code) -> HttpResponse {
  std::ostringstream stream;
  stream << "{\"error\":\"" << message << "\"}";
  return make_json_response(stream.str(), status_code);
}

void add_cors_headers(HttpResponse& response) {
  response.headers["Access-Control-Allow-Origin"] = "*";
  response.headers["Access-Control-Allow-Methods"] = "GET, POST, OPTIONS";
  response.headers["Access-Control-Allow-Headers"] = "Content-Type";
}

void serve_forever(const std::uint16_t port, const RequestHandler& handler) {
  const auto server_fd = ::socket(AF_INET6, SOCK_STREAM, 0);
  if (server_fd < 0) {
    throw std::runtime_error{std::string{"socket failed: "} + std::strerror(errno)};
  }

  const int reuse = 1;
  static_cast<void>(::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)));

  sockaddr_in6 address{};
  address.sin6_family = AF_INET6;
  address.sin6_addr = in6addr_any;
  address.sin6_port = htons(port);

  if (::bind(server_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
    close_socket(server_fd);
    throw std::runtime_error{std::string{"bind failed: "} + std::strerror(errno)};
  }

  if (::listen(server_fd, 64) < 0) {
    close_socket(server_fd);
    throw std::runtime_error{std::string{"listen failed: "} + std::strerror(errno)};
  }

  std::cout << "listening on port " << port << '\n';

  while (true) {
    sockaddr_storage client_address{};
    socklen_t client_address_length = sizeof(client_address);
    const auto client_fd = ::accept(server_fd, reinterpret_cast<sockaddr*>(&client_address), &client_address_length);
    if (client_fd < 0) {
      std::cerr << "accept failed: " << std::strerror(errno) << '\n';
      continue;
    }

    try {
      const auto raw_request = read_http_message(client_fd);
      const auto request = parse_request(raw_request);
      auto response = handler(request);
      add_cors_headers(response);
      send_all(client_fd, serialize_response(response));
    } catch (const std::exception& error) {
      auto response = make_error_response(error.what(), 500);
      add_cors_headers(response);
      send_all(client_fd, serialize_response(response));
    }

    close_socket(client_fd);
  }
}

[[nodiscard]] auto http_post(std::string url, std::string body, std::string content_type) -> HttpResponse {
  const auto parsed_url = parse_http_url(url);
  const auto socket_fd = connect_to_host(parsed_url.host, parsed_url.port);

  std::ostringstream request;
  request << "POST " << parsed_url.path << " HTTP/1.1\r\n";
  request << "Host: " << parsed_url.host << "\r\n";
  request << "Content-Type: " << content_type << "\r\n";
  request << "Content-Length: " << body.size() << "\r\n";
  request << "Connection: close\r\n\r\n";
  request << body;

  try {
    send_all(socket_fd, request.str());
    std::string raw_response;
    std::array<char, 4096U> buffer{};
    while (true) {
      const auto bytes_read = ::recv(socket_fd, buffer.data(), buffer.size(), 0);
      if (bytes_read <= 0) {
        break;
      }
      raw_response.append(buffer.data(), static_cast<std::size_t>(bytes_read));
    }
    close_socket(socket_fd);
    return parse_response(raw_response);
  } catch (...) {
    close_socket(socket_fd);
    throw;
  }
}

[[nodiscard]] auto getenv_or(std::string name, std::string fallback) -> std::string {
  const auto* value = std::getenv(name.c_str());
  if (value == nullptr || std::string_view{value}.empty()) {
    return fallback;
  }
  return value;
}

[[nodiscard]] auto getenv_u16_or(std::string name, const std::uint16_t fallback) -> std::uint16_t {
  const auto value = getenv_or(std::move(name), {});
  if (value.empty()) {
    return fallback;
  }

  unsigned int parsed{};
  const auto* begin = value.data();
  const auto* end = value.data() + value.size();
  const auto [ptr, error] = std::from_chars(begin, end, parsed);
  if (error != std::errc{} || ptr != end || parsed > 65535U) {
    return fallback;
  }
  return static_cast<std::uint16_t>(parsed);
}

}  // namespace optiflow::service
