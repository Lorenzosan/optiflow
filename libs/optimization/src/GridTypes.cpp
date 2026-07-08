#include "optiflow/numerics/GridTypes.h"

namespace optiflow::numerics {

StateIndex::StateIndex(std::size_t reservoir_index_value, std::size_t battery_index_value)
    : reservoir_index(reservoir_index_value), battery_index(battery_index_value) {}

GridBracket::GridBracket(std::size_t lower_index_value,
                         std::size_t upper_index_value,
                         double upper_weight_value)
    : lower_index(lower_index_value), upper_index(upper_index_value), upper_weight(upper_weight_value) {}

GridCell::GridCell(GridBracket reservoir_bracket_value, GridBracket battery_bracket_value)
    : reservoir_bracket(reservoir_bracket_value), battery_bracket(battery_bracket_value) {}

}  // namespace optiflow::numerics
