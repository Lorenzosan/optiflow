#include "optiflow/core/CsvScenarioReader.h"

#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace optiflow::core {

namespace {

std::string trim(const std::string& value) {
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
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
                    const std::string& file_label) {
    if (header != expected) {
        throw std::invalid_argument(file_label + " has an invalid header");
    }
}

double parse_double(const std::string& value, const std::string& key) {
    try {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid numeric value for " + key + ": " + value);
    }
}

std::size_t parse_size(const std::string& value, const std::string& key) {
    try {
        std::size_t consumed = 0;
        const unsigned long long parsed = std::stoull(value, &consumed);
        if (consumed != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        throw std::invalid_argument("invalid integer value for " + key + ": " + value);
    }
}

std::string required_string(const std::map<std::string, std::string>& values, const std::string& key) {
    const auto it = values.find(key);
    if (it == values.end() || trim(it->second).empty()) {
        throw std::invalid_argument("missing required key: " + key);
    }
    return trim(it->second);
}

double required_double(const std::map<std::string, std::string>& values, const std::string& key) {
    return parse_double(required_string(values, key), key);
}

std::size_t required_size(const std::map<std::string, std::string>& values, const std::string& key) {
    return parse_size(required_string(values, key), key);
}

std::vector<Exogenous> read_timeseries(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open time-series file: " + path.string());
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument("time-series file is empty");
    }
    require_header(split_csv_line(line), {"time_index", "price", "natural_inflow"}, "time-series file");

    std::vector<Exogenous> series;
    std::size_t expected_index = 0;
    std::size_t line_number = 1;

    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> columns = split_csv_line(line);
        if (columns.size() != 3) {
            throw std::invalid_argument("time-series line " + std::to_string(line_number) +
                                        " must have three columns");
        }

        const std::size_t time_index = parse_size(columns.at(0), "time_index");
        if (time_index != expected_index) {
            throw std::invalid_argument("time_index must start at zero and increase by one");
        }
        ++expected_index;

        series.emplace_back(parse_double(columns.at(1), "price"),
                            parse_double(columns.at(2), "natural_inflow"));
    }

    if (series.empty()) {
        throw std::invalid_argument("time-series file contains no data rows");
    }
    return series;
}

std::map<std::string, std::string> read_constraints(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open constraints file: " + path.string());
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument("constraints file is empty");
    }
    require_header(split_csv_line(line), {"key", "value"}, "constraints file");

    std::map<std::string, std::string> values;
    std::size_t line_number = 1;

    while (std::getline(input, line)) {
        ++line_number;
        if (trim(line).empty()) {
            continue;
        }
        const std::vector<std::string> columns = split_csv_line(line);
        if (columns.size() != 2) {
            throw std::invalid_argument("constraints line " + std::to_string(line_number) +
                                        " must have two columns");
        }
        const std::string key = trim(columns.at(0));
        const std::string value = trim(columns.at(1));
        if (key.empty()) {
            throw std::invalid_argument("constraints line " + std::to_string(line_number) +
                                        " has an empty key");
        }
        if (values.contains(key)) {
            throw std::invalid_argument("duplicate constraints key: " + key);
        }
        values.emplace(key, value);
    }
    return values;
}

}  // namespace

ScenarioBundle CsvScenarioReader::read(const std::filesystem::path& timeseries_path,
                                       const std::filesystem::path& constraints_path) {
    const std::vector<Exogenous> exogenous_series = read_timeseries(timeseries_path);
    const std::map<std::string, std::string> values = read_constraints(constraints_path);

    const ModelParameters model_parameters(required_double(values, "time_step_hours"),
                                           required_double(values, "reservoir_min_volume"),
                                           required_double(values, "reservoir_max_volume"),
                                           required_double(values, "battery_min_soc"),
                                           required_double(values, "battery_max_soc"),
                                           required_double(values, "turbine_max_flow"),
                                           required_double(values, "pump_max_flow"),
                                           required_double(values, "spill_max_flow"),
                                           required_double(values, "battery_max_charge_power"),
                                           required_double(values, "battery_max_discharge_power"),
                                           required_double(values, "turbine_efficiency"),
                                           required_double(values, "pump_efficiency"),
                                           required_double(values, "battery_charge_efficiency"),
                                           required_double(values, "battery_discharge_efficiency"),
                                           required_double(values, "water_to_power_factor"),
                                           required_double(values, "battery_degradation_cost_per_mwh"),
                                           required_double(values, "operating_cost_per_mwh"),
                                           required_double(values, "infeasibility_penalty"));

    const TerminalParameters terminal_parameters(required_double(values, "terminal_reservoir_min_volume"),
                                                 required_double(values, "terminal_reservoir_max_volume"),
                                                 required_double(values, "terminal_battery_min_soc"),
                                                 required_double(values, "terminal_battery_max_soc"),
                                                 required_double(values, "terminal_target_reservoir_volume"),
                                                 required_double(values, "terminal_target_battery_soc"),
                                                 required_double(values, "terminal_reservoir_target_penalty"),
                                                 required_double(values, "terminal_battery_target_penalty"));

    const SolverParameters solver_parameters(required_size(values, "reservoir_volume_grid_points"),
                                             required_size(values, "battery_soc_grid_points"),
                                             required_size(values, "turbine_flow_steps"),
                                             required_size(values, "spill_flow_steps"),
                                             required_size(values, "pump_flow_steps"),
                                             required_size(values, "battery_charge_steps"),
                                             required_size(values, "battery_discharge_steps"),
                                             required_double(values, "discount_factor"));

    const State initial_state(required_double(values, "initial_reservoir_volume"),
                              required_double(values, "initial_battery_soc"));

    Scenario scenario(required_string(values, "scenario_name"),
                      initial_state,
                      exogenous_series,
                      model_parameters,
                      terminal_parameters);

    return ScenarioBundle(std::move(scenario), solver_parameters);
}

}  // namespace optiflow::core
