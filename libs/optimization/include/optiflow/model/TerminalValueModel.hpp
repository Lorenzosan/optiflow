#pragma once

namespace optiflow {

/**
 * @brief Linear terminal value assigned to remaining reservoir water.
 */
class TerminalValueModel {
public:
    /**
     * @brief Construct a terminal value model.
     */
    explicit TerminalValueModel(double terminal_water_value_eur_per_m3);

    /**
     * @brief Return terminal value for a reservoir volume.
     */
    [[nodiscard]] double value(double reservoir_volume_m3) const noexcept;

private:
    double terminal_water_value_eur_per_m3_{};
};

} // namespace optiflow
