# OptiFlow model units and conventions

This document defines the units and sign conventions used by the C++ optimizer core. The current model is intentionally simplified for a local interview demo. It is not a calibrated hydraulic model.

## Time indexing

The optimization horizon has `N` decision steps indexed from `0` to `N - 1`. The value function has `N + 1` slices, including the terminal slice at index `N`.

`time_step_hours` is the duration of one decision step in hours. Price and inflow rows must use consecutive `time_index` values starting at zero.

## State variables

`reservoir_volume` is the amount of water stored in the upper reservoir, measured in a consistent abstract volume unit.

`battery_soc` is the battery state of charge, measured in MWh.

The state grid is a uniform two-dimensional grid over reservoir volume and battery state of charge. A no-battery case is represented by setting the battery minimum and maximum state of charge to zero and using one battery grid point.

## Action variables

`turbine_flow` is the water volume released through the turbine during one time step.

`pump_flow` is the water volume pumped into the upper reservoir during one time step.

`spill_flow` is the water volume released without producing electricity during one time step.

`battery_charge_power` is charging power in MW. Over one step it changes battery energy by `battery_charge_power * time_step_hours * battery_charge_efficiency`.

`battery_discharge_power` is discharging power in MW. Over one step it reduces battery energy by `battery_discharge_power * time_step_hours / battery_discharge_efficiency`.

The transition model rejects simultaneous turbine and pump operation, and simultaneous battery charge and discharge operation.

## Exogenous inputs

`price` is the electricity price, measured in currency per MWh. Negative prices are allowed.

`natural_inflow` is the natural water inflow into the upper reservoir during one time step, measured in the same abstract volume unit as `reservoir_volume`. It must be nonnegative.

## Power and reward

`turbine_power` is computed as:

```text
turbine_flow * water_to_power_factor * turbine_efficiency
```

`pump_power` is computed as:

```text
pump_flow * water_to_power_factor / pump_efficiency
```

`net_power` is positive when exporting to the market and negative when consuming from the market:

```text
turbine_power - pump_power - battery_charge_power + battery_discharge_power
```

The one-step reward is market revenue minus operating and degradation costs:

```text
price * net_power * time_step_hours
  - operating_cost_per_mwh * abs(net_power) * time_step_hours
  - battery_degradation_cost_per_mwh
      * (battery_charge_power + battery_discharge_power) * time_step_hours
```

The cumulative profit reported by the forward simulator is the running sum of one-step rewards.

## Terminal state

Terminal hard bounds define the allowed final reservoir volume and battery state of charge. A terminal state outside those bounds is infeasible for the Bellman terminal slice.

Terminal targets are soft preferences. They add a negative quadratic terminal value:

```text
- terminal_reservoir_target_penalty
    * (final_reservoir_volume - terminal_target_reservoir_volume)^2
- terminal_battery_target_penalty
    * (final_battery_soc - terminal_target_battery_soc)^2
```

Use wide terminal bounds and zero target penalties when no final-state preference is intended. Use narrow terminal bounds for hard final-state requirements. Use positive target penalties when deviations from a preferred final state should be discouraged but not forbidden.
