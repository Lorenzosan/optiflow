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
- Greedy forward simulation from the stored value function.
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
```

## Suggested next commits

1. Add unit tests for all infeasible transition cases.
2. Add JSON input after CSV is stable.
3. Add gRPC optimizer service.
4. Add persistence schema and API service.
5. Add Docker Compose and frontend last.
