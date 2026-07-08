# Deterministic pumped-storage model

This restart models a single upper reservoir with deterministic price and inflow inputs.

## State

```text
reservoir_volume_m3
```

## Controls

The first supported controls are operating modes:

```text
idle
turbine(q)
pump(q)
```

The model does not expose spill as an independent control yet. Overflow spill is derived when the raw next reservoir volume exceeds the maximum reservoir volume.

## Transition

```text
raw_next_volume =
    reservoir_volume
    + dt_seconds * natural_inflow_m3_s
    - dt_seconds * turbine_flow_m3_s
    + dt_seconds * pump_flow_m3_s

overflow_spill_m3 = max(0, raw_next_volume - max_reservoir_volume_m3)

next_volume = raw_next_volume - overflow_spill_m3
```

A transition is infeasible if `next_volume` is below the minimum reservoir volume.

## Power conversion

```text
turbine_power_mw = rho * g * head_m * turbine_efficiency * turbine_flow_m3_s / 1e6
pump_power_mw = rho * g * head_m * pump_flow_m3_s / (pump_efficiency * 1e6)
net_power_mw = turbine_power_mw - pump_power_mw
```

## Reward

```text
reward_eur = price_eur_per_mwh * net_power_mw * timestep_hours
             - overflow_spill_penalty_eur_per_m3 * overflow_spill_m3
```

## Terminal target penalty

The deterministic restart does not assign a positive economic value to remaining water. Instead, the terminal value is a non-positive penalty for missing a requested final reservoir volume.

```text
terminal_penalty_eur = terminal_reservoir_penalty_eur_per_m3
                       * abs(final_reservoir_volume_m3 - target_final_reservoir_volume_m3)

V_T(s) = -terminal_penalty_eur
```

This discourages artificial end-of-horizon draining without requiring the exact final reservoir target to be reachable on the discretized grid. The same pattern can later be extended to battery SOC.
