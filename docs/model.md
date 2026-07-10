# OptiFlow model units and conventions

OptiFlow models one pumped-storage hydro plant. The stored-energy inventory is the water held in the upper reservoir. There is no separate electrical storage state.

## Time indexing

The optimization horizon has `N` decisions indexed from `0` to `N - 1`. `time_step_hours` is the duration of one decision interval. Price and inflow files use consecutive `time_index` values starting at zero.

## State

The state is the upper-reservoir water inventory:

```text
state = reservoir_volume
```

`reservoir_volume` is measured in a consistent volume unit. With fixed hydraulic head, stored gravitational energy is proportional to this volume. For a closed-loop plant with a fixed total quantity of water, lower-reservoir volume is implied by water conservation and is not a second independent state.

## Actions

* `turbine_flow`: average water flow through the turbine, in volume units per hour.
* `pump_flow`: average water flow pumped into the upper reservoir, in volume units per hour.
* `spill_flow`: average water flow released without generation, in volume units per hour.

Actions are nonnegative. Turbine and pump operation cannot occur simultaneously.

## Exogenous inputs

* `price`: electricity price in currency per MWh. Negative prices are allowed.
* `natural_inflow`: average natural water inflow in volume units per hour. It must be nonnegative.

## State transition

```text
next_reservoir_volume = reservoir_volume
  + natural_inflow * time_step_hours
  + pump_flow * time_step_hours
  - turbine_flow * time_step_hours
  - spill_flow * time_step_hours
```

Both current and next reservoir volume must remain within configured hard bounds.

## Power and reward

```text
turbine_power = turbine_flow * water_to_power_factor * turbine_efficiency
pump_power = pump_flow * water_to_power_factor / pump_efficiency
net_power = turbine_power - pump_power
```

Positive `net_power` is export; negative `net_power` is import.

```text
reward = price * net_power * time_step_hours
  - operating_cost_per_mwh
      * (turbine_power + pump_power)
      * time_step_hours
```

`water_to_power_factor` uses a fixed-head approximation. A calibrated hydraulic model would make the conversion depend on head and plant characteristics.

## Terminal inventory

`terminal_reservoir_min_volume` and `terminal_reservoir_max_volume` are hard final-state bounds. `terminal_target_reservoir_volume` is a soft target with quadratic penalty:

```text
- terminal_reservoir_target_penalty
    * (final_reservoir_volume - terminal_target_reservoir_volume)^2
```

Set the penalty to zero when the target should not affect the objective.

## Numerical solver

The Bellman value function is tabulated on a one-dimensional uniform reservoir grid. Continuation values are linearly interpolated between adjacent reservoir grid points. The action grid is the Cartesian product of turbine, spill, and pump axes.

## Dispatch and diagnostics

The dispatch CSV contains state, hydraulic actions, net power, reward, and cumulative profit. Runner diagnostics include horizon size, reservoir grid size, action count, solve/simulation timings, import/export energy, final reservoir volume, and turbine/pump/spill/wait counters.

`tools/validate_dispatch.py` checks the generated trajectory against input series, grid actions, physical constraints, state transitions, power, reward, cumulative profit, terminal bounds, and optional CLI diagnostics.
