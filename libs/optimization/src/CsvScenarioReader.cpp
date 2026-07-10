#include "optiflow/core/CsvScenarioReader.h"

#include <array>
#include <set>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace optiflow::core {

namespace {

struct IndexedSeries {
    std::filesystem::path path;
    std::vector<std::size_t> line_numbers;
    std::vector<std::size_t> time_indices;
    std::vector<double> values;
};

struct ScenarioParameter {
    std::string value;
    std::size_t line_number;
};

using ScenarioParameterMap = std::map<std::string, ScenarioParameter>;

const std::set<std::string> allowed_scenario_keys = {
    "scenario_name",
    "time_step_hours",
    "reservoir_min_volume",
    "reservoir_max_volume",
    "turbine_max_flow",
    "pump_max_flow",
    "spill_max_flow",
    "turbine_efficiency",
    "pump_efficiency",
    "water_to_power_factor",
    "operating_cost_per_mwh",
    "infeasibility_penalty",
    "initial_reservoir_volume",
    "terminal_reservoir_min_volume",
    "terminal_reservoir_max_volume",
    "terminal_target_reservoir_volume",
    "terminal_reservoir_target_penalty",
    "reservoir_volume_grid_points",
    "turbine_flow_steps",
    "spill_flow_steps",
    "pump_flow_steps",
    "discount_factor",
    "market_start_utc",
    "market_timezone",
    "peak_start_hour",
    "peak_end_hour",
};

std::string trim(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    return value.substr(first, value.find_last_not_of(whitespace) - first + 1);
}

std::string location(const std::filesystem::path& path, std::size_t line_number) {
    return path.string() + ":" + std::to_string(line_number);
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> columns;
    std::stringstream stream(line);
    std::string item;
    while (std::getline(stream, item, ',')) {
        columns.push_back(trim(item));
    }
    return columns;
}

void require_header(const std::vector<std::string>& header,
                    const std::vector<std::string>& expected,
                    const std::filesystem::path& path,
                    const std::string& label) {
    if (header != expected) {
        throw std::invalid_argument(label + " file " + location(path, 1) +
                                    " has an invalid header");
    }
}

double parse_double(const std::string& value,
                    const std::string& key,
                    const std::string& context) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid numeric value for " + key + " at " + context +
                                    ": " + value);
    }
}

std::size_t parse_size(const std::string& value,
                       const std::string& key,
                       const std::string& context) {
    try {
        if (!value.empty() && value.front() == '-') {
            throw std::invalid_argument("negative value");
        }
        std::size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid integer value for " + key + " at " + context +
                                    ": " + value);
    }
}

std::string required_string(const ScenarioParameterMap& values,
                            const std::string& key,
                            const std::filesystem::path& path) {
    const auto iterator = values.find(key);
    if (iterator == values.end() || trim(iterator->second.value).empty()) {
        throw std::invalid_argument("missing required key in scenario file " + path.string() +
                                    ": " + key);
    }
    return trim(iterator->second.value);
}

double required_double(const ScenarioParameterMap& values,
                       const std::string& key,
                       const std::filesystem::path& path) {
    const auto iterator = values.find(key);
    return parse_double(required_string(values, key, path),
                        key,
                        location(path, iterator->second.line_number));
}

std::size_t required_size(const ScenarioParameterMap& values,
                          const std::string& key,
                          const std::filesystem::path& path) {
    const auto iterator = values.find(key);
    return parse_size(required_string(values, key, path),
                      key,
                      location(path, iterator->second.line_number));
}

IndexedSeries read_series(const std::filesystem::path& path,
                          const std::string& value_column,
                          const std::string& label) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open " + label + " file: " + path.string());
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument(label + " file " + path.string() + " is empty");
    }
    require_header(split_csv_line(line), {"time_index", value_column}, path, label);

    IndexedSeries series;
    series.path = path;
    std::size_t expected_index = 0;
    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> columns = split_csv_line(line);
        if (columns.size() != 2) {
            throw std::invalid_argument(label + " file " + location(path, line_number) +
                                        " must have two columns");
        }
        const std::string row_location = location(path, line_number);
        const std::size_t time_index = parse_size(columns.at(0), "time_index", row_location);
        if (time_index != expected_index) {
            throw std::invalid_argument(label + " file " + row_location +
                                        " time_index must start at zero and increase by one");
        }
        ++expected_index;
        const double value = parse_double(columns.at(1), value_column, row_location);
        if (value_column == "natural_inflow" && value < 0.0) {
            throw std::invalid_argument("natural_inflow must be nonnegative at " + row_location);
        }
        series.line_numbers.push_back(line_number);
        series.time_indices.push_back(time_index);
        series.values.push_back(value);
    }
    if (series.values.empty()) {
        throw std::invalid_argument(label + " file " + path.string() + " contains no data rows");
    }
    return series;
}

std::vector<Exogenous> combine_series(const IndexedSeries& prices,
                                      const IndexedSeries& inflows) {
    if (prices.values.size() != inflows.values.size()) {
        throw std::invalid_argument("price and inflow files must contain the same number of rows: " +
                                    prices.path.string() + " has " +
                                    std::to_string(prices.values.size()) + ", " +
                                    inflows.path.string() + " has " +
                                    std::to_string(inflows.values.size()));
    }

    std::vector<Exogenous> result;
    result.reserve(prices.values.size());
    for (std::size_t index = 0; index < prices.values.size(); ++index) {
        if (prices.time_indices.at(index) != inflows.time_indices.at(index)) {
            throw std::invalid_argument("price and inflow time_index values must match: " +
                                        location(prices.path, prices.line_numbers.at(index)) + " has " +
                                        std::to_string(prices.time_indices.at(index)) + ", " +
                                        location(inflows.path, inflows.line_numbers.at(index)) + " has " +
                                        std::to_string(inflows.time_indices.at(index)));
        }
        result.emplace_back(prices.values.at(index), inflows.values.at(index));
    }
    return result;
}

ScenarioParameterMap read_parameters(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open scenario file: " + path.string());
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument("scenario file " + path.string() + " is empty");
    }
    require_header(split_csv_line(line), {"key", "value"}, path, "scenario");

    ScenarioParameterMap values;
    std::size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> columns = split_csv_line(line);
        if (columns.size() != 2) {
            throw std::invalid_argument("scenario file " + location(path, line_number) +
                                        " must have two columns");
        }
        const std::string key = trim(columns.at(0));
        if (key.empty()) {
            throw std::invalid_argument("scenario file " + location(path, line_number) +
                                        " has an empty key");
        }
        if (values.contains(key)) {
            throw std::invalid_argument("duplicate scenario key in scenario file " +
                                        location(path, line_number) + ": " + key);
        }
        values.emplace(key, ScenarioParameter{trim(columns.at(1)), line_number});
    }
    return values;
}

void reject_unsupported_parameters(const ScenarioParameterMap& values,
                                   const std::filesystem::path& path) {
    for (const auto& [key, parameter] : values) {
        if (!allowed_scenario_keys.contains(key)) {
            throw std::invalid_argument("unsupported scenario key at " +
                                        location(path, parameter.line_number) + ": " + key);
        }
    }
}

}  // namespace

ScenarioBundle CsvScenarioReader::read(const std::filesystem::path& scenario_path,
                                       const std::filesystem::path& prices_path,
                                       const std::filesystem::path& inflows_path) {
    const IndexedSeries prices = read_series(prices_path, "price", "prices");
    const IndexedSeries inflows = read_series(inflows_path, "natural_inflow", "inflows");
    const std::vector<Exogenous> exogenous_series = combine_series(prices, inflows);
    const ScenarioParameterMap values = read_parameters(scenario_path);
    reject_unsupported_parameters(values, scenario_path);

    try {
        const ModelParameters model_parameters(
            required_double(values, "time_step_hours", scenario_path),
            required_double(values, "reservoir_min_volume", scenario_path),
            required_double(values, "reservoir_max_volume", scenario_path),
            required_double(values, "turbine_max_flow", scenario_path),
            required_double(values, "pump_max_flow", scenario_path),
            required_double(values, "spill_max_flow", scenario_path),
            required_double(values, "turbine_efficiency", scenario_path),
            required_double(values, "pump_efficiency", scenario_path),
            required_double(values, "water_to_power_factor", scenario_path),
            required_double(values, "operating_cost_per_mwh", scenario_path),
            required_double(values, "infeasibility_penalty", scenario_path));

        const TerminalParameters terminal_parameters(
            required_double(values, "terminal_reservoir_min_volume", scenario_path),
            required_double(values, "terminal_reservoir_max_volume", scenario_path),
            required_double(values, "terminal_target_reservoir_volume", scenario_path),
            required_double(values, "terminal_reservoir_target_penalty", scenario_path));

        const SolverParameters solver_parameters(
            required_size(values, "reservoir_volume_grid_points", scenario_path),
            required_size(values, "turbine_flow_steps", scenario_path),
            required_size(values, "spill_flow_steps", scenario_path),
            required_size(values, "pump_flow_steps", scenario_path),
            required_double(values, "discount_factor", scenario_path));

        const State initial_state(required_double(values, "initial_reservoir_volume", scenario_path));
        Scenario scenario(required_string(values, "scenario_name", scenario_path),
                          initial_state,
                          exogenous_series,
                          model_parameters,
                          terminal_parameters);
        return ScenarioBundle(std::move(scenario), solver_parameters);
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("scenario file " + scenario_path.string() + ": " + error.what());
    }
}

}  // namespace optiflow::core
