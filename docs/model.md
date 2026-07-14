# OptiFlow model units and conventions

OptiFlow models one pumped-storage hydro plant. The stored-energy inventory is the water held in the upper reservoir. There is no separate electrical storage state.

## Time indexing

The optimization horizon has `N` decisions indexed internally from `0` to `N - 1`. `time_step_hours` is the duration of one decision interval. Price and inflow files carry matching canonical UTC interval-start timestamps. Timestamps must be strictly increasing and spaced exactly by `time_step_hours`.


## Units and currency

OptiFlow uses the following explicit conventions without rescaling the existing numerical inputs:

* Reservoir volumes: `10³ m³`.
* Natural, turbine, pump, and spill flows: `10³ m³/h`.
* Power: `MW`; interval energy: `MWh`.
* Electricity prices and operating costs: `€/MWh`.
* Rewards, profit, and cash flows: `€`.
* Fixed hydraulic head: `146.79 m` (model constant).
* `terminal_reservoir_target_penalty`: `€/(10³ m³)²`.

## State

The state is the upper-reservoir water inventory:

```text
state = reservoir_volume
```

`reservoir_volume` is measured in thousands of cubic metres (`10³ m³`). With fixed hydraulic head, stored gravitational energy is proportional to this volume. For a closed-loop plant with a fixed total quantity of water, lower-reservoir volume is implied by water conservation and is not a second independent state.

## Actions

* `turbine_flow`: average water flow through the turbine, in `10³ m³/h`.
* `pump_flow`: average water flow pumped into the upper reservoir, in `10³ m³/h`.
* `spill_flow`: average water flow released without generation, in `10³ m³/h`.

Actions are nonnegative. Turbine and pump operation cannot occur simultaneously.

## Exogenous inputs

* `price`: electricity price in euros per MWh (`€/MWh`). Negative prices are allowed.
* `natural_inflow`: average natural water inflow in `10³ m³/h`. It must be nonnegative.

## State transition

```text
next_reservoir_volume = reservoir_volume
  + natural_inflow * time_step_hours
  + pump_flow * time_step_hours
  - turbine_flow * time_step_hours
  - spill_flow * time_step_hours
```

Both current and next reservoir volume must remain within configured hard bounds. Actions that violate physical limits or reservoir bounds are infeasible and are excluded from the Bellman maximization; they do not receive a tunable objective penalty.

## Power and reward

```text
hydraulic_power_factor = ρ * g * fixed_hydraulic_head
  * 1000 m³ per flow unit / 3600 s per hour / 10⁶ W per MW
turbine_power = turbine_flow * hydraulic_power_factor * turbine_efficiency
pump_power = pump_flow * hydraulic_power_factor / pump_efficiency
net_power = turbine_power - pump_power
```

Positive `net_power` is export; negative `net_power` is import.

```text
reward = price * net_power * time_step_hours
  - operating_cost_per_mwh
      * (turbine_power + pump_power)
      * time_step_hours
```

The fixed head is 146.79 m, which gives a hydraulic power factor of 0.4 MW per `10³ m³/h`. It is derived internally rather than supplied by each scenario. A calibrated hydraulic model would make head depend on reservoir level and hydraulic losses. Legacy scenario files may still contain `water_to_power_factor,0.4`; the reader accepts that exact value but ignores it.

## Terminal inventory

`terminal_reservoir_min_volume` and `terminal_reservoir_max_volume` are hard final-state bounds. `terminal_target_reservoir_volume` is a soft target with quadratic penalty:

```text
- terminal_reservoir_target_penalty
    * (final_reservoir_volume - terminal_target_reservoir_volume)^2
```

Set the penalty to zero when the target should not affect the objective.

## Numerical solver

The Bellman value function is tabulated on a one-dimensional uniform reservoir grid. Continuation values are linearly interpolated between adjacent reservoir grid points. The action grid is generated from turbine, spill, and pump axes, then pruned so mutually exclusive turbine and pump controls are never stored. Zero-range axes collapse to a single zero control.

## Dispatch and diagnostics

The dispatch CSV contains the generated `time_index`, authoritative `timestamp_utc`, state, hydraulic actions, net power, reward, and cumulative profit. Runner diagnostics include horizon size, reservoir grid size, action count, solve/simulation timings, import/export energy, final reservoir volume, and turbine/pump/spill/wait counters.

`tools/validate_dispatch.py` checks the generated trajectory against input series, grid actions, physical constraints, state transitions, power, reward, cumulative profit, terminal bounds, and optional CLI diagnostics.
