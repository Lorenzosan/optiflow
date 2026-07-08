#include "optiflow/demo/CsvScenarioLoader.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
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

void validate_header(const std::vector<std::string>& header,
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
      validate_header(split_csv_line(line), path, value_column_name);
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

}  // namespace optiflow::demo
