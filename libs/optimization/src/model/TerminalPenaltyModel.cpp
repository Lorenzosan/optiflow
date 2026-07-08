#include <optiflow/model/TerminalPenaltyModel.hpp>

#include <cmath>

namespace optiflow {

TerminalPenaltyModel::TerminalPenaltyModel(double target_reservoir_volume_m3,
                                           double penalty_eur_per_m3)
    : target_reservoir_volume_m3_(target_reservoir_volume_m3),
      penalty_eur_per_m3_(penalty_eur_per_m3) {}

double TerminalPenaltyModel::value(double reservoir_volume_m3) const noexcept {
    return -penalty_cost_eur(reservoir_volume_m3);
}

double TerminalPenaltyModel::deviation_m3(double reservoir_volume_m3) const noexcept {
    return std::abs(reservoir_volume_m3 - target_reservoir_volume_m3_);
}

double TerminalPenaltyModel::penalty_cost_eur(double reservoir_volume_m3) const noexcept {
    return penalty_eur_per_m3_ * deviation_m3(reservoir_volume_m3);
}

} // namespace optiflow
