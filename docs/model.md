# Pumped-storage model

OptiFlow uses a constant-head hydraulic-energy representation. Physical water volumes are converted outside the optimizer using a plant-specific representative head. The optimizer itself does not expose hydraulic head or a water-to-power coefficient.

## Units

* Reservoir content and terminal values: `MWh hydraulic`.
* Natural inflow, turbine withdrawal, pump addition, and spill: `MW hydraulic`.
* Generated, consumed, and net electrical power: `MW electric`.
* Electricity price and operating cost: `€/MWh`.
* Market settlement, operating cost, interval reward, and cumulative model reward: `€`.
* Terminal target penalty: `€/MWh²`.

## State and controls

The state is the hydraulic energy stored in the upper reservoir:

```text
state = reservoir_volume  # MWh hydraulic
```

The historical field name `reservoir_volume` is retained in the CSV/API for compatibility, but its value is an energy equivalent. The controls likewise retain their existing field names while representing hydraulic power:

* `turbine_flow`: hydraulic power withdrawn from storage, in MW.
* `pump_flow`: hydraulic power added to storage, in MW.
* `spill_flow`: hydraulic power discarded, in MW.
* `natural_inflow`: exogenous hydraulic power entering storage, in MW.

Turbine withdrawal and spill may occur together, for example when inflow exceeds turbine capacity. Pumping is mutually exclusive with both turbine withdrawal and spill; the model never pumps water into the upper reservoir while releasing it.

## Reservoir transition

```text
next_reservoir_volume = reservoir_volume
  + natural_inflow * time_step_hours
  + pump_flow * time_step_hours
  - turbine_flow * time_step_hours
  - spill_flow * time_step_hours
```

Because power multiplied by hours gives energy, the next state remains in MWh hydraulic.

## Electrical conversion

```text
turbine_power = turbine_flow * turbine_efficiency
pump_power = pump_flow / pump_efficiency
net_power = turbine_power - pump_power
```

Efficiencies are dimensionless fractions in `(0, 1]`. Hydraulic head is assumed constant during conversion from physical plant data into the scenario's MW/MWh values.

## Economics

```text
market_settlement = electricity_price * net_power * time_step_hours
operating_cost = operating_cost_per_mwh
  * (turbine_power + pump_power)
  * time_step_hours
interval_reward = market_settlement - operating_cost
```

The reporting layer presents market settlement, modeled operating cost, and net operating cashflow per interval. The persisted `cumulative_profit` field is the cumulative model reward retained for schema compatibility. The terminal target penalty affects the Bellman objective but is not included in reported net operating cashflow.

## Terminal conditions

`terminal_reservoir_min_volume` and `terminal_reservoir_max_volume` are hard final-state bounds in MWh hydraulic. `terminal_target_reservoir_volume` is a soft target with quadratic penalty:

```text
terminal_penalty = terminal_reservoir_target_penalty
  * (final_reservoir_volume - terminal_target_reservoir_volume)^2
```

## Numerical resolution and equal-value actions

The Bellman solver evaluates a discrete hydraulic-action grid at every reservoir-grid state. Linear interpolation supplies continuation values for next states between reservoir grid points. Numerical results should therefore be checked across nested state and action resolutions rather than treated as independent of discretization.

For a storage range of width `W` and `N` grid points, spacing is `W / (N - 1)`. The browser editor therefore asks for interval count and writes `intervals + 1` points into the optimizer schema. This avoids the common off-by-one case where entering `100` points over a `0–200` MWh range creates `2.0202` MWh spacing instead of the intended `2` MWh spacing.

A quadratic terminal target can magnify interpolation error. For penalty coefficient `p` and grid spacing `h`, the largest gap between the quadratic terminal value and its linear interpolation within one cell is `p h² / 4`. If that amount is comparable to or larger than interval cashflows, dispatch timing can be dominated by grid alignment. Use nested resolution studies and action-aligned grids to assess this effect; the browser editor intentionally does not present a dynamic resolution diagnostic.

`tools/analyze_resolution.py` creates scenario variants with nested solver grids and reports each result relative to the finest listed case. The comparison is a sensitivity diagnostic. It does not assume that profit increases monotonically, because changing the reservoir grid also changes interpolation error.

A strictly larger Bellman value always replaces the current action. Exactly equal values use an explicit deterministic tie-break: less spill, then less total controlled hydraulic throughput, then less pumping, then less turbine withdrawal. The same rule is used in backward induction and value-function forward simulation, so equal-value dispatch does not depend on action enumeration order.
