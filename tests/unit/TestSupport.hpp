#pragma once

#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

#define OPTIFLOW_REQUIRE(condition)                                                                 \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            throw std::runtime_error(std::string("requirement failed: ") + #condition);             \
        }                                                                                           \
    } while (false)

#define OPTIFLOW_REQUIRE_NEAR(actual, expected, tolerance)                                           \
    do {                                                                                            \
        if (std::fabs((actual) - (expected)) > (tolerance)) {                                        \
            throw std::runtime_error(std::string("near requirement failed: ") + #actual);            \
        }                                                                                           \
    } while (false)

template <typename Function>
int run_test(Function&& function) {
    try {
        function();
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "test failed: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

template <typename Function>
void require_throws(Function&& function) {
    bool did_throw = false;
    try {
        function();
    } catch (const std::exception&) {
        did_throw = true;
    }
    if (!did_throw) {
        throw std::runtime_error("expected exception was not thrown");
    }
}

inline std::filesystem::path write_temp_file(const std::string& name, const std::string& content) {
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream output(path);
    output << content;
    return path;
}
