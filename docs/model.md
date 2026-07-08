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

## Terminal value

```text
V_T(s) = terminal_water_value_eur_per_m3 * reservoir_volume_m3
```

Terminal value avoids the common artifact where the optimizer drains the reservoir at the end of the horizon only because leftover water has zero modeled value.
