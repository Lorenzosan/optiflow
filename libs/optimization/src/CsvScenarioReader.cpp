#include "optiflow/core/CsvScenarioReader.h"


#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace optiflow::core {

namespace {

struct TimestampValue {
    std::string text;
    std::chrono::sys_seconds instant;
};

struct TimestampedSeries {
    std::filesystem::path path;
    std::vector<std::size_t> line_numbers;
    std::vector<TimestampValue> timestamps;
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
    "operating_cost_per_mwh",
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
        if (consumed != value.size() || !std::isfinite(parsed)) {
            throw std::invalid_argument("invalid numeric value");
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
    const std::string value = required_string(values, key, path);
    const auto iterator = values.find(key);
    return parse_double(value, key, location(path, iterator->second.line_number));
}

std::size_t required_size(const ScenarioParameterMap& values,
                          const std::string& key,
                          const std::filesystem::path& path) {
    const std::string value = required_string(values, key, path);
    const auto iterator = values.find(key);
    return parse_size(value, key, location(path, iterator->second.line_number));
}

int parse_timestamp_component(const std::string& value,
                              std::size_t offset,
                              std::size_t length,
                              const std::string& context) {
    int result = 0;
    for (std::size_t index = offset; index < offset + length; ++index) {
        const char character = value.at(index);
        if (character < '0' || character > '9') {
            throw std::invalid_argument(
                "timestamp_utc must use YYYY-MM-DDTHH:MM:SSZ at " + context + ": " + value);
        }
        result = result * 10 + (character - '0');
    }
    return result;
}

TimestampValue parse_timestamp(const std::string& value, const std::string& context) {
    if (value.size() != 20 || value.at(4) != '-' || value.at(7) != '-' ||
        value.at(10) != 'T' || value.at(13) != ':' || value.at(16) != ':' ||
        value.at(19) != 'Z') {
        throw std::invalid_argument(
            "timestamp_utc must use YYYY-MM-DDTHH:MM:SSZ at " + context + ": " + value);
    }

    const int year_value = parse_timestamp_component(value, 0, 4, context);
    const unsigned int month_value =
        static_cast<unsigned int>(parse_timestamp_component(value, 5, 2, context));
    const unsigned int day_value =
        static_cast<unsigned int>(parse_timestamp_component(value, 8, 2, context));
    const int hour_value = parse_timestamp_component(value, 11, 2, context);
    const int minute_value = parse_timestamp_component(value, 14, 2, context);
    const int second_value = parse_timestamp_component(value, 17, 2, context);

    const std::chrono::year_month_day date{
        std::chrono::year(year_value),
        std::chrono::month(month_value),
        std::chrono::day(day_value)};
    if (!date.ok() || hour_value > 23 || minute_value > 59 || second_value > 59) {
        throw std::invalid_argument("invalid timestamp_utc at " + context + ": " + value);
    }

    const std::chrono::sys_seconds instant =
        std::chrono::sys_days(date) + std::chrono::hours(hour_value) +
        std::chrono::minutes(minute_value) + std::chrono::seconds(second_value);
    return TimestampValue{value, instant};
}

std::chrono::seconds interval_seconds(double time_step_hours,
                                      const std::filesystem::path& scenario_path) {
    const double seconds_value = time_step_hours * 3600.0;
    const double maximum = static_cast<double>(std::numeric_limits<long long>::max());
    if (!std::isfinite(seconds_value) || seconds_value <= 0.0 || seconds_value > maximum) {
        throw std::invalid_argument("scenario file " + scenario_path.string() +
                                    ": time_step_hours is outside the supported timestamp range");
    }
    const long long rounded_seconds = std::llround(seconds_value);
    if (std::abs(seconds_value - static_cast<double>(rounded_seconds)) > 1.0e-9) {
        throw std::invalid_argument(
            "scenario file " + scenario_path.string() +
            ": time_step_hours must resolve to a whole number of seconds for timestamped series");
    }
    return std::chrono::seconds(rounded_seconds);
}

TimestampedSeries read_series(const std::filesystem::path& path,
                              const std::string& value_column,
                              const std::string& label,
                              std::chrono::seconds expected_interval) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open " + label + " file: " + path.string());
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::invalid_argument(label + " file " + path.string() + " is empty");
    }
    require_header(split_csv_line(line), {"timestamp_utc", value_column}, path, label);

    TimestampedSeries series;
    series.path = path;
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
        const TimestampValue timestamp = parse_timestamp(columns.at(0), row_location);
        if (!series.timestamps.empty()) {
            const TimestampValue& previous = series.timestamps.back();
            if (timestamp.instant <= previous.instant) {
                throw std::invalid_argument(label + " file " + row_location +
                                            " timestamps must be strictly increasing");
            }
            if (timestamp.instant - previous.instant != expected_interval) {
                throw std::invalid_argument(
                    label + " file " + row_location +
                    " timestamp spacing must equal scenario time_step_hours");
            }
        }
        const double value = parse_double(columns.at(1), value_column, row_location);
        if (value_column == "natural_inflow" && value < 0.0) {
            throw std::invalid_argument("natural_inflow must be nonnegative at " + row_location);
        }
        series.line_numbers.push_back(line_number);
        series.timestamps.push_back(timestamp);
        series.values.push_back(value);
    }
    if (series.values.empty()) {
        throw std::invalid_argument(label + " file " + path.string() + " contains no data rows");
    }
    return series;
}

std::vector<Exogenous> combine_series(const TimestampedSeries& prices,
                                      const TimestampedSeries& inflows) {
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
        const TimestampValue& price_timestamp = prices.timestamps.at(index);
        const TimestampValue& inflow_timestamp = inflows.timestamps.at(index);
        if (price_timestamp.text != inflow_timestamp.text) {
            throw std::invalid_argument(
                "price and inflow timestamp_utc values must match: " +
                location(prices.path, prices.line_numbers.at(index)) + " has " +
                price_timestamp.text + ", " +
                location(inflows.path, inflows.line_numbers.at(index)) + " has " +
                inflow_timestamp.text);
        }
        result.emplace_back(price_timestamp.text,
                            prices.values.at(index),
                            inflows.values.at(index));
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


template <typename Factory>
auto scenario_value(const std::filesystem::path& path, Factory&& factory) {
    try {
        return factory();
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument("scenario file " + path.string() + ": " + error.what());
    }
}

}  // namespace

ScenarioBundle CsvScenarioReader::read(const std::filesystem::path& scenario_path,
                                       const std::filesystem::path& prices_path,
                                       const std::filesystem::path& inflows_path) {
    const ScenarioParameterMap values = read_parameters(scenario_path);
    reject_unsupported_parameters(values, scenario_path);

    const ModelParameters model_parameters = scenario_value(scenario_path, [&]() {
        return ModelParameters(
            required_double(values, "time_step_hours", scenario_path),
            required_double(values, "reservoir_min_volume", scenario_path),
            required_double(values, "reservoir_max_volume", scenario_path),
            required_double(values, "turbine_max_flow", scenario_path),
            required_double(values, "pump_max_flow", scenario_path),
            required_double(values, "spill_max_flow", scenario_path),
            required_double(values, "turbine_efficiency", scenario_path),
            required_double(values, "pump_efficiency", scenario_path),
            required_double(values, "operating_cost_per_mwh", scenario_path));
    });
    scenario_value(scenario_path, [&]() {
        validate_model_parameters(model_parameters);
        return true;
    });

    const std::chrono::seconds expected_interval =
        interval_seconds(model_parameters.time_step_hours, scenario_path);
    const TimestampedSeries prices =
        read_series(prices_path, "price", "prices", expected_interval);
    const TimestampedSeries inflows =
        read_series(inflows_path, "natural_inflow", "inflows", expected_interval);
    const std::vector<Exogenous> exogenous_series = combine_series(prices, inflows);

    const TerminalParameters terminal_parameters = scenario_value(scenario_path, [&]() {
        return TerminalParameters(
            required_double(values, "terminal_reservoir_min_volume", scenario_path),
            required_double(values, "terminal_reservoir_max_volume", scenario_path),
            required_double(values, "terminal_target_reservoir_volume", scenario_path),
            required_double(values, "terminal_reservoir_target_penalty", scenario_path));
    });
    const SolverParameters solver_parameters = scenario_value(scenario_path, [&]() {
        return SolverParameters(
            required_size(values, "reservoir_volume_grid_points", scenario_path),
            required_size(values, "turbine_flow_steps", scenario_path),
            required_size(values, "spill_flow_steps", scenario_path),
            required_size(values, "pump_flow_steps", scenario_path),
            required_double(values, "discount_factor", scenario_path));
    });
    const State initial_state = scenario_value(scenario_path, [&]() {
        return State(required_double(values, "initial_reservoir_volume", scenario_path));
    });
    const std::string name = scenario_value(scenario_path, [&]() {
        return required_string(values, "scenario_name", scenario_path);
    });

    Scenario scenario = scenario_value(scenario_path, [&]() {
        return Scenario(name,
                        initial_state,
                        exogenous_series,
                        model_parameters,
                        terminal_parameters);
    });
    return scenario_value(scenario_path, [&]() {
        return ScenarioBundle(std::move(scenario), solver_parameters);
    });
}

}  // namespace optiflow::core
