#include <optiflow/io/CsvReader.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace optiflow {
namespace {

std::string strip_cr(std::string line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return line;
}

std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, ',')) {
        fields.push_back(field);
    }
    if (!line.empty() && line.back() == ',') {
        fields.emplace_back();
    }
    return fields;
}

std::size_t parse_time_index(const std::string& text, const std::string& path, std::size_t row) {
    try {
        std::size_t consumed = 0;
        const auto value = std::stoll(text, &consumed);
        if (consumed != text.size() || value < 0) {
            throw std::invalid_argument("invalid time index");
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception&) {
        throw std::runtime_error(path + ": invalid time_index at row " + std::to_string(row));
    }
}

double parse_double(const std::string& text, const std::string& path, std::size_t row,
                    const std::string& column) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return value;
    } catch (const std::exception&) {
        throw std::runtime_error(path + ": invalid " + column + " at row " + std::to_string(row));
    }
}

std::map<std::size_t, double> read_two_column_csv(const std::filesystem::path& path,
                                                  const std::string& expected_header,
                                                  const std::string& value_column,
                                                  bool reject_negative_values) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("could not open CSV file: " + path.string());
    }

    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("empty CSV file: " + path.string());
    }

    line = strip_cr(line);
    if (line != expected_header) {
        throw std::runtime_error(path.string() + ": expected header '" + expected_header + "'");
    }

    std::map<std::size_t, double> values;
    std::size_t row = 1;
    while (std::getline(input, line)) {
        ++row;
        line = strip_cr(line);
        if (line.empty()) {
            continue;
        }

        const auto fields = split_csv_line(line);
        if (fields.size() != 2) {
            throw std::runtime_error(path.string() + ": malformed row " + std::to_string(row));
        }

        const auto time_index = parse_time_index(fields[0], path.string(), row);
        const auto value = parse_double(fields[1], path.string(), row, value_column);

        if (reject_negative_values && value < 0.0) {
            throw std::runtime_error(path.string() + ": negative " + value_column + " at row " +
                                     std::to_string(row));
        }

        const auto [_, inserted] = values.emplace(time_index, value);
        if (!inserted) {
            throw std::runtime_error(path.string() + ": duplicate time_index " +
                                     std::to_string(time_index));
        }
    }

    if (values.empty()) {
        throw std::runtime_error("CSV file contains no data rows: " + path.string());
    }

    return values;
}

} // namespace

DeterministicSeries read_deterministic_series(const std::filesystem::path& price_csv_path,
                                              const std::filesystem::path& inflow_csv_path) {
    const auto prices = read_two_column_csv(price_csv_path, "time_index,price_eur_per_mwh",
                                            "price_eur_per_mwh", false);
    const auto inflows = read_two_column_csv(inflow_csv_path, "time_index,natural_inflow_m3_s",
                                             "natural_inflow_m3_s", true);

    if (prices.size() != inflows.size()) {
        throw std::runtime_error("price and inflow CSV files have different number of time steps");
    }

    DeterministicSeries series;
    series.points.reserve(prices.size());

    for (const auto& [time_index, price] : prices) {
        const auto inflow_it = inflows.find(time_index);
        if (inflow_it == inflows.end()) {
            throw std::runtime_error("missing inflow for time_index " + std::to_string(time_index));
        }
        series.points.push_back(ExogenousPoint{time_index, price, inflow_it->second});
    }

    for (const auto& [time_index, _] : inflows) {
        if (prices.find(time_index) == prices.end()) {
            throw std::runtime_error("missing price for time_index " + std::to_string(time_index));
        }
    }

    return series;
}

} // namespace optiflow
