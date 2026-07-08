#include "optiflow/demo/CsvScenarioLoader.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace optiflow::demo {
namespace {

struct CsvRecord final {
  std::size_t time_index{};
  double value{};
};

struct StochasticCsvRecord final {
  std::size_t time_index{};
  std::size_t realization_index{};
  double probability{};
  double value{};
};

constexpr double probability_tolerance = 1.0e-9;

[[nodiscard]] auto trim(const std::string& input) -> std::string {
  auto begin = input.begin();
  while (begin != input.end() && std::isspace(static_cast<unsigned char>(*begin)) != 0) {
    ++begin;
  }

  auto end = input.end();
  while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))) != 0) {
    --end;
  }

  return std::string{begin, end};
}

[[nodiscard]] auto split_csv_line(const std::string& line) -> std::vector<std::string> {
  std::vector<std::string> fields;
  std::stringstream stream{line};
  std::string field;

  while (std::getline(stream, field, ',')) {
    fields.push_back(trim(field));
  }

  if (!line.empty() && line.back() == ',') {
    fields.emplace_back();
  }

  return fields;
}

[[nodiscard]] auto parse_time_index(const std::string& text,
                                    const std::string& path,
                                    const std::size_t line_number) -> std::size_t {
  try {
    if (text.empty() || text.front() == '-') {
      throw std::invalid_argument{"negative or empty time index"};
    }

    std::size_t parsed_chars = 0U;
    const auto value = std::stoull(text, &parsed_chars);
    if (parsed_chars != text.size()) {
      throw std::invalid_argument{"trailing characters"};
    }
    return static_cast<std::size_t>(value);
  } catch (const std::exception&) {
    throw std::runtime_error{path + ":" + std::to_string(line_number) + ": invalid time index '" + text + "'"};
  }
}

[[nodiscard]] auto parse_double(const std::string& text,
                                const std::string& path,
                                const std::size_t line_number,
                                const std::string& column_name) -> double {
  try {
    std::size_t parsed_chars = 0U;
    const auto value = std::stod(text, &parsed_chars);
    if (parsed_chars != text.size()) {
      throw std::invalid_argument{"trailing characters"};
    }
    return value;
  } catch (const std::exception&) {
    throw std::runtime_error{path + ":" + std::to_string(line_number) + ": invalid " + column_name + " '" + text + "'"};
  }
}

void validate_deterministic_header(const std::vector<std::string>& header,
                                   const std::string& path,
                                   const std::string& value_column_name) {
  if (header.size() != 2U) {
    throw std::runtime_error{path + ": expected exactly two CSV columns"};
  }

  const auto has_time_column = header[0] == "time_index" || header[0] == "hour";
  if (!has_time_column || header[1] != value_column_name) {
    throw std::runtime_error{path + ": expected header time_index," + value_column_name};
  }
}

[[nodiscard]] auto read_csv_series(const std::string& path,
                                   const std::string& value_column_name) -> std::vector<double> {
  std::ifstream file{path};
  if (!file.is_open()) {
    throw std::runtime_error{"could not open CSV file: " + path};
  }

  std::string line;
  std::size_t line_number = 0U;

  while (std::getline(file, line)) {
    ++line_number;
    if (!trim(line).empty()) {
      validate_deterministic_header(split_csv_line(line), path, value_column_name);
      break;
    }
  }

  if (line_number == 0U || trim(line).empty()) {
    throw std::runtime_error{path + ": empty CSV file"};
  }

  std::vector<CsvRecord> records;
  while (std::getline(file, line)) {
    ++line_number;
    if (trim(line).empty()) {
      continue;
    }

    const auto fields = split_csv_line(line);
    if (fields.size() != 2U) {
      throw std::runtime_error{path + ":" + std::to_string(line_number) + ": expected exactly two CSV fields"};
    }

    records.push_back(CsvRecord{
        .time_index = parse_time_index(fields[0], path, line_number),
        .value = parse_double(fields[1], path, line_number, value_column_name),
    });
  }

  if (records.empty()) {
    throw std::runtime_error{path + ": CSV file has no data rows"};
  }

  std::sort(records.begin(), records.end(), [](const CsvRecord& lhs, const CsvRecord& rhs) {
    return lhs.time_index < rhs.time_index;
  });

  std::vector<double> values;
  values.reserve(records.size());
  for (std::size_t index = 0U; index < records.size(); ++index) {
    if (records[index].time_index != index) {
      throw std::runtime_error{path + ": time_index values must be contiguous and start at 0"};
    }
    values.push_back(records[index].value);
  }


  return values;
}

void validate_stochastic_header(const std::vector<std::string>& header,
                                const std::string& path,
                                const std::string& value_column_name) {
  if (header.size() != 4U) {
    throw std::runtime_error{path + ": expected exactly four CSV columns"};
  }

  const auto has_time_column = header[0] == "time_index" || header[0] == "hour";
  const auto has_realization_column = header[1] == "realization_index" || header[1] == "outcome_index";
  if (!has_time_column || !has_realization_column || header[2] != "probability" ||
      header[3] != value_column_name) {
    throw std::runtime_error{
        path + ": expected header time_index,realization_index,probability," + value_column_name};
  }
}

void validate_probability(const double probability, const std::string& path, const std::size_t line_number) {
  if (!std::isfinite(probability) || probability < 0.0) {
    throw std::runtime_error{path + ":" + std::to_string(line_number) + ": probability must be finite and non-negative"};
  }
}

[[nodiscard]] auto read_stochastic_csv_series(const std::string& path,
                                              const std::string& value_column_name)
    -> std::vector<StochasticCsvRecord> {
  std::ifstream file{path};
  if (!file.is_open()) {
    throw std::runtime_error{"could not open CSV file: " + path};
  }

  std::string line;
  std::size_t line_number = 0U;

  while (std::getline(file, line)) {
    ++line_number;
    if (!trim(line).empty()) {
      validate_stochastic_header(split_csv_line(line), path, value_column_name);
      break;
    }
  }

  if (line_number == 0U || trim(line).empty()) {
    throw std::runtime_error{path + ": empty CSV file"};
  }

  std::vector<StochasticCsvRecord> records;
  while (std::getline(file, line)) {
    ++line_number;
    if (trim(line).empty()) {
      continue;
    }

    const auto fields = split_csv_line(line);
    if (fields.size() != 4U) {
      throw std::runtime_error{path + ":" + std::to_string(line_number) + ": expected exactly four CSV fields"};
    }

    const auto probability = parse_double(fields[2], path, line_number, "probability");
    validate_probability(probability, path, line_number);

    records.push_back(StochasticCsvRecord{
        .time_index = parse_time_index(fields[0], path, line_number),
        .realization_index = parse_time_index(fields[1], path, line_number),
        .probability = probability,
        .value = parse_double(fields[3], path, line_number, value_column_name),
    });
  }

  if (records.empty()) {
    throw std::runtime_error{path + ": CSV file has no data rows"};
  }

  std::sort(records.begin(), records.end(), [](const StochasticCsvRecord& lhs, const StochasticCsvRecord& rhs) {
    if (lhs.time_index != rhs.time_index) {
      return lhs.time_index < rhs.time_index;
    }
    return lhs.realization_index < rhs.realization_index;
  });

  auto expected_time_index = std::size_t{0U};
  auto record_index = std::size_t{0U};
  while (record_index < records.size()) {
    const auto time_index = records[record_index].time_index;
    if (time_index != expected_time_index) {
      throw std::runtime_error{path + ": time_index values must be contiguous and start at 0"};
    }

    auto expected_realization_index = std::size_t{0U};
    auto probability_sum = 0.0;
    while (record_index < records.size() && records[record_index].time_index == time_index) {
      const auto& record = records[record_index];
      if (record.realization_index != expected_realization_index) {
        throw std::runtime_error{
            path + ": realization_index values must be contiguous and start at 0 within each time step"};
      }
      probability_sum += record.probability;
      ++expected_realization_index;
      ++record_index;
    }

    if (std::abs(probability_sum - 1.0) > probability_tolerance) {
      throw std::runtime_error{path + ": probabilities must sum to one within each time step"};
    }

    ++expected_time_index;
  }

  return records;
}

void require_matching_stochastic_key(const StochasticCsvRecord& price_record,
                                     const StochasticCsvRecord& inflow_record) {
  if (price_record.time_index != inflow_record.time_index ||
      price_record.realization_index != inflow_record.realization_index) {
    throw std::runtime_error{"stochastic price and inflow CSV files must use the same time and realization indexes"};
  }

  if (std::abs(price_record.probability - inflow_record.probability) > probability_tolerance) {
    throw std::runtime_error{"stochastic price and inflow CSV files must assign the same probability to each realization"};
  }
}

}  // namespace

[[nodiscard]] auto load_deterministic_exogenous_csv(const std::string& price_csv_path,
                                                    const std::string& inflow_csv_path)
    -> std::vector<Exogenous> {
  const auto prices = read_csv_series(price_csv_path, "price_eur_per_mwh");
  const auto inflows = read_csv_series(inflow_csv_path, "natural_inflow_m3_s");

  if (prices.size() != inflows.size()) {
    throw std::runtime_error{"price and inflow CSV files must contain the same number of time steps"};
  }

  std::vector<Exogenous> exogenous;
  exogenous.reserve(prices.size());
  for (std::size_t index = 0U; index < prices.size(); ++index) {
    exogenous.push_back(Exogenous{
        .price_eur_per_mwh = prices[index],
        .natural_inflow_m3_s = inflows[index],
    });
  }

  return exogenous;
}

[[nodiscard]] auto load_stochastic_exogenous_csv(const std::string& price_csv_path,
                                                 const std::string& inflow_csv_path)
    -> StochasticExogenousProcess {
  const auto price_records = read_stochastic_csv_series(price_csv_path, "price_eur_per_mwh");
  const auto inflow_records = read_stochastic_csv_series(inflow_csv_path, "natural_inflow_m3_s");

  if (price_records.size() != inflow_records.size()) {
    throw std::runtime_error{"stochastic price and inflow CSV files must contain the same number of realizations"};
  }

  const auto horizon = price_records.back().time_index + 1U;
  auto process = StochasticExogenousProcess{};
  process.resize(horizon);

  for (std::size_t index = 0U; index < price_records.size(); ++index) {
    const auto& price_record = price_records[index];
    const auto& inflow_record = inflow_records[index];
    require_matching_stochastic_key(price_record, inflow_record);

    process[price_record.time_index].push_back(WeightedExogenous{
        .value = Exogenous{
            .price_eur_per_mwh = price_record.value,
            .natural_inflow_m3_s = inflow_record.value,
        },
        .probability = price_record.probability,
    });
  }

  return process;
}

}  // namespace optiflow::demo
