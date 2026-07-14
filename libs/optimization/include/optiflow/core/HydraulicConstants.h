#pragma once

namespace optiflow::core {

inline constexpr double water_density_kg_per_cubic_metre = 1000.0;
inline constexpr double gravitational_acceleration_metres_per_second_squared = 9.81;
inline constexpr double fixed_hydraulic_head_metres = 146.78899082568807;
inline constexpr double cubic_metres_per_flow_unit = 1000.0;
inline constexpr double seconds_per_hour = 3600.0;
inline constexpr double watts_per_megawatt = 1.0e6;
inline constexpr double hydraulic_power_factor_mw_per_flow_unit =
    water_density_kg_per_cubic_metre *
    gravitational_acceleration_metres_per_second_squared *
    fixed_hydraulic_head_metres * cubic_metres_per_flow_unit /
    (seconds_per_hour * watts_per_megawatt);

}  // namespace optiflow::core
