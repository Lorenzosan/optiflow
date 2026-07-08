# Optimization model

## State

```text
s_t = (reservoir_volume_m3, battery_soc_mwh)
```

## Action

```text
a_t = (turbine_flow_m3_s,
       spill_flow_m3_s,
       pump_flow_m3_s,
       battery_charge_mw,
       battery_discharge_mw)
```

## Exogenous input

```text
w_t = (price_eur_per_mwh, natural_inflow_m3_s)
```

## Transition

The physical model computes:

```text
s_{t+1} = f(s_t, a_t, w_t)
```

The simplified model enforces reservoir and battery bounds and rejects infeasible actions.

## Reward

```text
reward = market revenue
       - hydro operating cost
       - battery degradation cost
       - spill penalty
```

## Deterministic Bellman recursion

```text
V_t(s) = max_a [ r(s, a, w_t) + gamma V_{t+1}(f(s, a, w_t)) ]
```

`V_{t+1}` is bilinearly interpolated at off-grid next states.

## Terminal value

The terminal value approximates continuation value beyond the finite horizon:

```text
terminal_value = water_value * reservoir_volume + battery_value * battery_soc
```

This is a modeling choice. It should be calibrated or stress-tested because it can strongly influence end-of-horizon behavior.

## Key limitations

- Fixed hydraulic head.
- Simplified turbine and pump efficiency.
- Simplified battery degradation.
- No ramping, minimum up/down time, startup cost, or unit commitment.
- Discrete state and action grids.
- Nearest-grid policy lookup in forward simulation.
- No market gate closure or ancillary-service products.
