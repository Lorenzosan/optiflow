#include <optiflow/model/TerminalValueModel.hpp>

namespace optiflow {

TerminalValueModel::TerminalValueModel(double terminal_water_value_eur_per_m3)
    : terminal_water_value_eur_per_m3_(terminal_water_value_eur_per_m3) {}

double TerminalValueModel::value(double reservoir_volume_m3) const noexcept {
    return terminal_water_value_eur_per_m3_ * reservoir_volume_m3;
}

} // namespace optiflow
