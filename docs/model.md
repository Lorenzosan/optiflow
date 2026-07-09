# OptiFlow model units and conventions

This document defines the units and sign conventions used by the C++ optimizer core. The current model is intentionally simplified for a local interview demo. It is not a calibrated hydraulic model.

## Time indexing

The optimization horizon has `N` decision steps indexed from `0` to `N - 1`. The value function has `N + 1` slices, including the terminal slice at index `N`.

`time_step_hours` is the duration of one decision step in hours. Price and inflow rows must use consecutive `time_index` values starting at zero. The yearly CSV example uses 8760 hourly rows, corresponding to one non-leap year.

## State variables

`reservoir_volume` is the amount of water stored in the upper reservoir, measured in a consistent abstract volume unit.

`battery_soc` is the battery state of charge, measured in MWh.

The state grid is a uniform two-dimensional grid over reservoir volume and battery state of charge. A no-battery case is represented by setting the battery minimum and maximum state of charge to zero and using one battery grid point.

## Action variables

`turbine_flow` is the average water flow released through the turbine during one time step, measured in reservoir-volume units per hour. Over one step it changes reservoir volume by `turbine_flow * time_step_hours`.

`pump_flow` is the average water flow pumped into the upper reservoir during one time step, measured in reservoir-volume units per hour. Over one step it changes reservoir volume by `pump_flow * time_step_hours`.

`spill_flow` is the average water flow released without producing electricity during one time step, measured in reservoir-volume units per hour. Over one step it changes reservoir volume by `spill_flow * time_step_hours`.

`battery_charge_power` is charging power in MW. Over one step it changes battery energy by `battery_charge_power * time_step_hours * battery_charge_efficiency`.

`battery_discharge_power` is discharging power in MW. Over one step it reduces battery energy by `battery_discharge_power * time_step_hours / battery_discharge_efficiency`.

The transition model rejects simultaneous turbine and pump operation, and simultaneous battery charge and discharge operation.

## Exogenous inputs

`price` is the electricity price, measured in currency per MWh. Negative prices are allowed.

`natural_inflow` is the average natural water inflow into the upper reservoir during one time step, measured in reservoir-volume units per hour. Over one step it changes reservoir volume by `natural_inflow * time_step_hours`. It must be nonnegative.

## State transition

Reservoir volume is updated as:

```text
next_reservoir_volume = reservoir_volume
  + natural_inflow * time_step_hours
  + pump_flow * time_step_hours
  - turbine_flow * time_step_hours
  - spill_flow * time_step_hours
```

Battery state of charge is updated as:

```text
next_battery_soc = battery_soc
  + battery_charge_power * battery_charge_efficiency * time_step_hours
  - battery_discharge_power / battery_discharge_efficiency * time_step_hours
```

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
  - operating_cost_per_mwh
      * (turbine_power + pump_power + battery_charge_power + battery_discharge_power)
      * time_step_hours
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

## Optimization diagnostics

`OptimizationRunner` returns diagnostics together with the dispatch trajectory and cumulative profit. The CLI prints these diagnostics to stdout after writing the dispatch CSV. The dispatch CSV remains trajectory-only and does not include run metadata. Future service adapters should expose the same runner diagnostics instead of recomputing solver metadata. These diagnostics are deterministic metadata except for wall-clock timings.

The structural fields are:

```text
horizon_steps
reservoir_grid_points
battery_grid_points
action_count
```

`horizon_steps` is the number of decision steps in the scenario. `reservoir_grid_points` and `battery_grid_points` are the state-grid dimensions used by the Bellman solver. `action_count` is the Cartesian product of the turbine, spill, pump, battery-charge, and battery-discharge action axes.

The timing fields are:

```text
solve_seconds
simulation_seconds
```

They are measured inside `OptimizationRunner` around the Bellman solve and the forward simulation. They are intended for local diagnostics and smoke checks, not for strict numerical regression tests. Regression tests should only require timing fields to be finite and nonnegative.

The activity-count fields are computed from the forward-simulated dispatch trajectory:

```text
turbine_steps
pump_steps
spill_steps
battery_charge_steps
battery_discharge_steps
wait_steps
```

Each non-wait counter is incremented when the corresponding action component is positive in a dispatch row. `wait_steps` is incremented only when turbine, pump, spill, battery charge, and battery discharge actions are all zero. Tests recompute these counters from dispatch rows and compare them with runner diagnostics. The CLI integration tests check that the short sample run prints the diagnostic labels and preserves the dispatch CSV header. A separate yearly-example test solves the 8760-step synthetic scenario and verifies that the generated dispatch file has one header plus 8760 trajectory rows.

The optional `tools/validate_dispatch.py` helper validates a generated dispatch CSV against the scenario, price, and inflow inputs. It checks row counts, consecutive time indices, state continuity, action-grid membership, state bounds, mutual-exclusion constraints, transition equations, net power, reward, cumulative profit, terminal hard bounds, and optional CLI diagnostic counters. The helper uses tolerances because the CLI writes floating-point values with stream-default precision. It is a trajectory consistency check; it does not replace small oracle tests for Bellman optimality.

The optional `tools/summarize_dispatch.py` helper reads the same scenario, price, inflow, and dispatch files and decomposes the trajectory into economic and energy-flow metrics. It reports export revenue, import cost, net market cashflow, operating cost, battery degradation cost, recomputed reward, energy imports and exports, weighted average operating prices, action counts, and final inventory relative to the configured terminal targets. It is deliberately separate from the validator: the validator answers whether the trajectory is internally consistent, while the summary explains the scale and structure of the result.


The optional `tools/compare_scenarios.py` helper runs the CLI for multiple scenario CSV files on a shared price and inflow series, then writes a compact comparison CSV. It is intended for sensitivity checks such as comparing the base yearly scenario with `examples/yearly/scenario_no_battery.csv`. The comparison table is not a replacement for dispatch validation; it assumes each generated dispatch can still be checked with `tools/validate_dispatch.py`.

The yearly comparison can include `examples/yearly/scenario_no_battery.csv` and `examples/yearly/scenario_high_battery_degradation.csv`. The first removes the battery physically; the second keeps it available but makes battery throughput expensive. This separates physical battery availability from economic battery use under the same price and inflow series.
