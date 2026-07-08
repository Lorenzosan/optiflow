# OptiFlow starting point

OptiFlow is a C++20 interview project for deterministic dispatch optimization of a pumped-storage asset with an optional battery.

This starting point intentionally contains only the optimization library, a CSV-driven CLI, examples, Doxygen configuration, and minimal tests. It does not include gRPC, REST, PostgreSQL, NGINX, Docker, or a frontend yet.

## What is implemented

- Explicit scenario loading from CSV files.
- No default model or solver parameters.
- 2D state grid: reservoir volume x battery state of charge.
- Uniform action grid generated from explicit limits and explicit step counts.
- Pumped-storage and battery transition model.
- Deterministic Bellman dynamic-programming solver.
- Bilinear interpolation of the continuation value.
- Terminal-state hard bounds and soft target penalties.
- Value-function-based forward simulation.
- Nearest-policy forward simulation for grid-aligned verification.
- CSV dispatch output.
- Doxygen comments on public interfaces.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run the sample

```bash
./build/apps/solve_cli/optiflow_solve \
  --timeseries examples/price_inflow.csv \
  --constraints examples/constraints.csv \
  --output build/dispatch.csv
```

The output file contains the dispatch trajectory with state, action, net power, reward, and cumulative profit.

## Generate documentation

Doxygen is configured but not required for the build.

```bash
doxygen Doxyfile
```

Generated HTML documentation will be written under `docs/html`.

## Input files

The time-series CSV must contain:

```csv
time_index,price,natural_inflow
```

The constraints CSV must contain:

```csv
key,value
```

Every required key must be present. Missing keys cause the CLI to fail. This is intentional.

Terminal-state behavior is controlled by explicit keys in the constraints file:

```csv
terminal_reservoir_min_volume,0
terminal_reservoir_max_volume,200
terminal_battery_min_soc,0
terminal_battery_max_soc,50
terminal_target_reservoir_volume,50
terminal_target_battery_soc,0
terminal_reservoir_target_penalty,0
terminal_battery_target_penalty,0
```

The terminal min and max values are hard final-state bounds. The target values are soft preferences. The penalty coefficients multiply squared deviations from those targets. Set a penalty to zero when the corresponding soft target should not affect the objective.

For a no-battery case, set:

```csv
battery_min_soc,0
battery_max_soc,0
battery_max_charge_power,0
battery_max_discharge_power,0
battery_soc_grid_points,1
battery_charge_steps,1
battery_discharge_steps,1
initial_battery_soc,0
terminal_battery_min_soc,0
terminal_battery_max_soc,0
terminal_target_battery_soc,0
terminal_battery_target_penalty,0
```

## Suggested next commits

1. Add JSON input after CSV is stable.
2. Add gRPC optimizer service.
3. Add persistence schema and API service.
4. Add Docker Compose and frontend last.
