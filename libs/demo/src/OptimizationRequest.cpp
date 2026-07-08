#include "optiflow/demo/OptimizationRequest.h"

#include "optiflow/demo/DemoScenario.h"

#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace optiflow::demo {
namespace {

class JsonValue final {
public:
  using Array = std::vector<JsonValue>;
  using Object = std::map<std::string, JsonValue>;

  JsonValue() = default;
  explicit JsonValue(std::nullptr_t) : m_value(nullptr) {}
  explicit JsonValue(bool value) : m_value(value) {}
  explicit JsonValue(double value) : m_value(value) {}
  explicit JsonValue(std::string value) : m_value(std::move(value)) {}
  explicit JsonValue(Array value) : m_value(std::move(value)) {}
  explicit JsonValue(Object value) : m_value(std::move(value)) {}

  [[nodiscard]] auto is_null() const noexcept -> bool { return std::holds_alternative<std::nullptr_t>(m_value); }
  [[nodiscard]] auto is_object() const noexcept -> bool { return std::holds_alternative<Object>(m_value); }

  [[nodiscard]] auto as_number(std::string_view context) const -> double {
    if (!std::holds_alternative<double>(m_value)) {
      throw std::invalid_argument{std::string{context} + " must be a number"};
    }
    return std::get<double>(m_value);
  }

  [[nodiscard]] auto as_string(std::string_view context) const -> const std::string& {
    if (!std::holds_alternative<std::string>(m_value)) {
      throw std::invalid_argument{std::string{context} + " must be a string"};
    }
    return std::get<std::string>(m_value);
  }

  [[nodiscard]] auto as_array(std::string_view context) const -> const Array& {
    if (!std::holds_alternative<Array>(m_value)) {
      throw std::invalid_argument{std::string{context} + " must be an array"};
    }
    return std::get<Array>(m_value);
  }

  [[nodiscard]] auto as_object(std::string_view context) const -> const Object& {
    if (!std::holds_alternative<Object>(m_value)) {
      throw std::invalid_argument{std::string{context} + " must be an object"};
    }
    return std::get<Object>(m_value);
  }

private:
  std::variant<std::nullptr_t, bool, double, std::string, Array, Object> m_value{nullptr};
};

class JsonParser final {
public:
  explicit JsonParser(std::string_view text) : m_text(text) {}

  [[nodiscard]] auto parse() -> JsonValue {
    skip_space();
    if (is_end()) {
      throw std::invalid_argument{"JSON body is empty"};
    }
    auto value = parse_value();
    skip_space();
    if (!is_end()) {
      throw std::invalid_argument{"unexpected trailing characters in JSON body"};
    }
    return value;
  }

private:
  [[nodiscard]] auto is_end() const noexcept -> bool { return m_position >= m_text.size(); }

  [[nodiscard]] auto peek() const -> char {
    if (is_end()) {
      throw std::invalid_argument{"unexpected end of JSON body"};
    }
    return m_text[m_position];
  }

  auto consume() -> char {
    const auto result = peek();
    ++m_position;
    return result;
  }

  void expect(const char expected) {
    if (consume() != expected) {
      throw std::invalid_argument{std::string{"expected '"} + expected + "' in JSON body"};
    }
  }

  [[nodiscard]] auto match(std::string_view token) -> bool {
    if (m_text.substr(m_position, token.size()) != token) {
      return false;
    }
    m_position += token.size();
    return true;
  }

  void skip_space() noexcept {
    while (!is_end()) {
      const auto character = m_text[m_position];
      if (character != ' ' && character != '\n' && character != '\r' && character != '\t') {
        return;
      }
      ++m_position;
    }
  }

  [[nodiscard]] auto parse_value() -> JsonValue {
    skip_space();
    const auto character = peek();
    if (character == '{') {
      return JsonValue{parse_object()};
    }
    if (character == '[') {
      return JsonValue{parse_array()};
    }
    if (character == '"') {
      return JsonValue{parse_string()};
    }
    if (character == '-' || (character >= '0' && character <= '9')) {
      return JsonValue{parse_number()};
    }
    if (match("true")) {
      return JsonValue{true};
    }
    if (match("false")) {
      return JsonValue{false};
    }
    if (match("null")) {
      return JsonValue{nullptr};
    }
    throw std::invalid_argument{"unexpected token in JSON body"};
  }

  [[nodiscard]] auto parse_object() -> JsonValue::Object {
    JsonValue::Object object;
    expect('{');
    skip_space();
    if (!is_end() && peek() == '}') {
      consume();
      return object;
    }

    while (true) {
      skip_space();
      const auto key = parse_string();
      skip_space();
      expect(':');
      auto value = parse_value();
      object.insert_or_assign(key, std::move(value));
      skip_space();
      const auto separator = consume();
      if (separator == '}') {
        return object;
      }
      if (separator != ',') {
        throw std::invalid_argument{"expected ',' or '}' in JSON object"};
      }
    }
  }

  [[nodiscard]] auto parse_array() -> JsonValue::Array {
    JsonValue::Array array;
    expect('[');
    skip_space();
    if (!is_end() && peek() == ']') {
      consume();
      return array;
    }

    while (true) {
      array.push_back(parse_value());
      skip_space();
      const auto separator = consume();
      if (separator == ']') {
        return array;
      }
      if (separator != ',') {
        throw std::invalid_argument{"expected ',' or ']' in JSON array"};
      }
    }
  }

  [[nodiscard]] auto parse_string() -> std::string {
    expect('"');
    std::string result;
    while (!is_end()) {
      const auto character = consume();
      if (character == '"') {
        return result;
      }
      if (character != '\\') {
        result.push_back(character);
        continue;
      }

      const auto escaped = consume();
      switch (escaped) {
        case '"':
        case '\\':
        case '/':
          result.push_back(escaped);
          break;
        case 'b':
          result.push_back('\b');
          break;
        case 'f':
          result.push_back('\f');
          break;
        case 'n':
          result.push_back('\n');
          break;
        case 'r':
          result.push_back('\r');
          break;
        case 't':
          result.push_back('\t');
          break;
        default:
          throw std::invalid_argument{"unsupported escape sequence in JSON string"};
      }
    }
    throw std::invalid_argument{"unterminated JSON string"};
  }

  [[nodiscard]] auto parse_number() -> double {
    const auto begin = m_position;
    if (!is_end() && peek() == '-') {
      consume();
    }
    while (!is_end() && peek() >= '0' && peek() <= '9') {
      consume();
    }
    if (!is_end() && peek() == '.') {
      consume();
      while (!is_end() && peek() >= '0' && peek() <= '9') {
        consume();
      }
    }
    if (!is_end() && (peek() == 'e' || peek() == 'E')) {
      consume();
      if (!is_end() && (peek() == '+' || peek() == '-')) {
        consume();
      }
      while (!is_end() && peek() >= '0' && peek() <= '9') {
        consume();
      }
    }

    const auto token = std::string{m_text.substr(begin, m_position - begin)};
    std::size_t parsed_size{};
    double value{};
    try {
      value = std::stod(token, &parsed_size);
    } catch (const std::exception&) {
      throw std::invalid_argument{"invalid JSON number"};
    }
    if (parsed_size != token.size() || !std::isfinite(value)) {
      throw std::invalid_argument{"invalid JSON number"};
    }
    return value;
  }

  std::string_view m_text;
  std::size_t m_position{};
};

[[nodiscard]] auto trim_view(std::string_view text) noexcept -> std::string_view {
  while (!text.empty()) {
    const auto front = text.front();
    if (front != ' ' && front != '\n' && front != '\r' && front != '\t') {
      break;
    }
    text.remove_prefix(1U);
  }
  while (!text.empty()) {
    const auto back = text.back();
    if (back != ' ' && back != '\n' && back != '\r' && back != '\t') {
      break;
    }
    text.remove_suffix(1U);
  }
  return text;
}

[[nodiscard]] auto find_member(const JsonValue::Object& object, std::string_view key) -> const JsonValue* {
  const auto iterator = object.find(std::string{key});
  if (iterator == object.end()) {
    return nullptr;
  }
  return &iterator->second;
}

[[nodiscard]] auto require_member(const JsonValue::Object& object,
                                  std::string_view key,
                                  std::string_view context) -> const JsonValue& {
  const auto* value = find_member(object, key);
  if (value == nullptr) {
    throw std::invalid_argument{std::string{context} + " is missing required field '" + std::string{key} + "'"};
  }
  return *value;
}

[[nodiscard]] auto optional_size_index(const JsonValue::Object& object,
                                       std::string_view key,
                                       std::string_view context) -> std::optional<std::size_t> {
  const auto* value = find_member(object, key);
  if (value == nullptr || value->is_null()) {
    return std::nullopt;
  }

  const auto number = value->as_number(std::string{context} + "." + std::string{key});
  if (number < 0.0 || std::floor(number) != number) {
    throw std::invalid_argument{std::string{context} + "." + std::string{key} + " must be a non-negative integer"};
  }
  return static_cast<std::size_t>(number);
}

[[nodiscard]] auto finite_number(const JsonValue::Object& object,
                                 std::string_view key,
                                 std::string_view context) -> double {
  const auto value = require_member(object, key, context).as_number(std::string{context} + "." + std::string{key});
  if (!std::isfinite(value)) {
    throw std::invalid_argument{std::string{context} + "." + std::string{key} + " must be finite"};
  }
  return value;
}

void validate_probability_sum(const StageDistribution& distribution, const std::size_t time_index) {
  double sum{};
  for (const auto& realization : distribution) {
    if (realization.probability < 0.0 || !std::isfinite(realization.probability)) {
      throw std::invalid_argument{"stochastic probability must be finite and non-negative"};
    }
    sum += realization.probability;
  }

  if (std::abs(sum - 1.0) > 1.0e-9) {
    throw std::invalid_argument{"stochastic probabilities at time_index " + std::to_string(time_index)
                                + " must sum to 1.0"};
  }
}

[[nodiscard]] auto parse_solver_kind(const JsonValue::Object& root, const bool has_stochastic_process) -> RequestSolverKind {
  const auto* value = find_member(root, "solver_kind");
  if (value == nullptr || value->is_null()) {
    return has_stochastic_process ? RequestSolverKind::Stochastic : RequestSolverKind::Deterministic;
  }

  const auto& text = value->as_string("solver_kind");
  if (text == "deterministic") {
    return RequestSolverKind::Deterministic;
  }
  if (text == "stochastic" || text == "stochastic_stagewise") {
    return RequestSolverKind::Stochastic;
  }
  throw std::invalid_argument{"solver_kind must be 'deterministic' or 'stochastic'"};
}

[[nodiscard]] auto parse_exogenous_array(const JsonValue::Object& root) -> std::vector<Exogenous> {
  const auto* value = find_member(root, "exogenous");
  if (value == nullptr || value->is_null()) {
    return {};
  }

  const auto& array = value->as_array("exogenous");
  if (array.empty()) {
    throw std::invalid_argument{"exogenous must contain at least one time step"};
  }

  std::vector<Exogenous> result;
  result.reserve(array.size());
  for (std::size_t index = 0U; index < array.size(); ++index) {
    const auto context = "exogenous[" + std::to_string(index) + "]";
    const auto& row = array[index].as_object(context);
    const auto time_index = optional_size_index(row, "time_index", context);
    if (time_index.has_value() && *time_index != index) {
      throw std::invalid_argument{"exogenous time_index must be contiguous and ordered from 0"};
    }

    result.push_back(Exogenous{
        .price_eur_per_mwh = finite_number(row, "price_eur_per_mwh", context),
        .natural_inflow_m3_s = finite_number(row, "natural_inflow_m3_s", context),
    });
  }

  return result;
}

[[nodiscard]] auto parse_stochastic_process(const JsonValue::Object& root) -> StochasticExogenousProcess {
  const auto* value = find_member(root, "stochastic_process");
  if (value == nullptr || value->is_null()) {
    return {};
  }

  const auto& stages = value->as_array("stochastic_process");
  if (stages.empty()) {
    throw std::invalid_argument{"stochastic_process must contain at least one time step"};
  }

  StochasticExogenousProcess process;
  process.reserve(stages.size());
  for (std::size_t time_index = 0U; time_index < stages.size(); ++time_index) {
    const auto stage_context = "stochastic_process[" + std::to_string(time_index) + "]";
    const auto& stage = stages[time_index].as_object(stage_context);
    const auto declared_time_index = optional_size_index(stage, "time_index", stage_context);
    if (declared_time_index.has_value() && *declared_time_index != time_index) {
      throw std::invalid_argument{"stochastic_process time_index must be contiguous and ordered from 0"};
    }

    const auto& realizations = require_member(stage, "realizations", stage_context).as_array(stage_context + ".realizations");
    if (realizations.empty()) {
      throw std::invalid_argument{stage_context + ".realizations must contain at least one realization"};
    }

    StageDistribution distribution;
    distribution.reserve(realizations.size());
    for (std::size_t realization_index = 0U; realization_index < realizations.size(); ++realization_index) {
      const auto realization_context = stage_context + ".realizations[" + std::to_string(realization_index) + "]";
      const auto& realization = realizations[realization_index].as_object(realization_context);
      const auto declared_realization_index = optional_size_index(realization, "realization_index", realization_context);
      if (declared_realization_index.has_value() && *declared_realization_index != realization_index) {
        throw std::invalid_argument{"realization_index must be contiguous and ordered from 0 within each stage"};
      }

      distribution.push_back(WeightedExogenous{
          .value = Exogenous{
              .price_eur_per_mwh = finite_number(realization, "price_eur_per_mwh", realization_context),
              .natural_inflow_m3_s = finite_number(realization, "natural_inflow_m3_s", realization_context),
          },
          .probability = finite_number(realization, "probability", realization_context),
      });
    }

    validate_probability_sum(distribution, time_index);
    process.push_back(std::move(distribution));
  }

  return process;
}

}  // namespace

[[nodiscard]] auto parse_optimization_request_json(std::string_view json) -> OptimizationRequest {
  json = trim_view(json);
  if (json.empty()) {
    return OptimizationRequest{
        .solver_kind = RequestSolverKind::Deterministic,
        .exogenous = make_default_exogenous(),
        .stochastic_process = {},
    };
  }

  const auto root_value = JsonParser{json}.parse();
  const auto& root = root_value.as_object("optimization request");
  if (root.empty()) {
    return OptimizationRequest{
        .solver_kind = RequestSolverKind::Deterministic,
        .exogenous = make_default_exogenous(),
        .stochastic_process = {},
    };
  }

  auto request = OptimizationRequest{};
  request.exogenous = parse_exogenous_array(root);
  request.stochastic_process = parse_stochastic_process(root);
  request.solver_kind = parse_solver_kind(root, !request.stochastic_process.empty());

  if (request.solver_kind == RequestSolverKind::Deterministic && request.exogenous.empty()) {
    throw std::invalid_argument{"deterministic optimization requires a non-empty exogenous array"};
  }
  if (request.solver_kind == RequestSolverKind::Stochastic && request.stochastic_process.empty()) {
    throw std::invalid_argument{"stochastic optimization requires a non-empty stochastic_process array"};
  }

  return request;
}

}  // namespace optiflow::demo
