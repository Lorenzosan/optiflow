#pragma once

namespace optiflow {

/**
 * @brief Penalizes deviation from a target final reservoir volume.
 *
 * The deterministic restart uses a terminal penalty instead of a terminal
 * water value. The penalty discourages ending the horizon far away from a
 * requested final physical state without making the problem infeasible when
 * the exact target is unreachable on the discretized grid.
 */
class TerminalPenaltyModel {
public:
    /**
     * @brief Construct a terminal penalty model.
     *
     * @param target_reservoir_volume_m3 Desired final reservoir volume.
     * @param penalty_eur_per_m3 Linear penalty for each cubic meter of final
     *        deviation from the target.
     */
    TerminalPenaltyModel(double target_reservoir_volume_m3, double penalty_eur_per_m3);

    /**
     * @brief Return terminal value for a reservoir volume.
     *
     * The returned value is non-positive and equals zero at the target final
     * reservoir volume.
     */
    [[nodiscard]] double value(double reservoir_volume_m3) const noexcept;

    /**
     * @brief Return the absolute deviation from the target final volume.
     */
    [[nodiscard]] double deviation_m3(double reservoir_volume_m3) const noexcept;

    /**
     * @brief Return the non-negative terminal penalty cost.
     */
    [[nodiscard]] double penalty_cost_eur(double reservoir_volume_m3) const noexcept;

private:
    double target_reservoir_volume_m3_{};
    double penalty_eur_per_m3_{};
};

} // namespace optiflow
