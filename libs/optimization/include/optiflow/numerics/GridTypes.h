#pragma once

#include <cstddef>

namespace optiflow::numerics {

/** @brief Integer index of one reservoir-volume grid point. */
struct StateIndex {
    std::size_t reservoir_index;

    explicit StateIndex(std::size_t reservoir_index);
};

/** @brief One-dimensional interpolation bracket. */
struct GridBracket {
    std::size_t lower_index;
    std::size_t upper_index;
    double upper_weight;

    GridBracket(std::size_t lower_index,
                std::size_t upper_index,
                double upper_weight);
};

}  // namespace optiflow::numerics
