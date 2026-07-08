# OptiFlow

OptiFlow is a deterministic C++20 dynamic-programming demo for pumped-storage hydro dispatch over a finite price and inflow horizon.

This restart intentionally focuses on the deterministic model core. It does not include stochastic optimization, frontend code, services, PostgreSQL, gRPC, or Docker integration yet.

## Current scope

- Read deterministic price and inflow CSV files.
- Validate physical and numerical model constraints.
- Build a reservoir state grid and a hydro action grid.
- Solve a deterministic Bellman recursion.
- Simulate the selected policy forward from the initial reservoir volume.
- Provide Doxygen-ready public headers.
- Provide unit tests for CSV loading, model validation, numerics, Bellman recursion, and forward simulation.

## Model

The state is the upper-reservoir volume:

```text
s_t = reservoir_volume_m3
```

The deterministic exogenous input is:

```text
x_t = price_eur_per_mwh_t, natural_inflow_m3_s_t
```

The first supported hydro controls are:

```text
idle
turbine(q)
pump(q)
```

Overflow spill is derived from the reservoir capacity. It is not a free control in this first restart.

The Bellman recursion is:

```text
V_t(s) = max_a [ r(s, a, x_t) + gamma V_{t+1}(f(s, a, x_t)) ]
```

## CSV formats

Price CSV:

```csv
time_index,price_eur_per_mwh
0,40.0
1,35.0
```

Inflow CSV:

```csv
time_index,natural_inflow_m3_s
0,15.0
1,12.0
```

The reader joins both files by `time_index`. The reader is strict:

- headers must match exactly;
- duplicate time indices are invalid;
- price may be negative;
- inflow must be non-negative;
- both files must contain the same time index set;
- the resulting series is sorted by `time_index`.

## Build

```bash
cmake -S . -B build \
  -DOPTIFLOW_BUILD_CLI=ON \
  -DOPTIFLOW_BUILD_TESTS=ON \
  -DOPTIFLOW_BUILD_DOCS=OFF

cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run CLI

```bash
./build/apps/optiflow_cli/optiflow_cli \
  --prices samples/prices_24h.csv \
  --inflows samples/inflows_24h.csv \
  --initial-reservoir-volume 500000 \
  --min-reservoir-volume 100000 \
  --max-reservoir-volume 1000000 \
  --max-turbine-flow 80 \
  --max-pump-flow 60 \
  --hydraulic-head 300 \
  --turbine-efficiency 0.9 \
  --pump-efficiency 0.85 \
  --timestep-hours 1 \
  --discount-factor 1 \
  --terminal-water-value 0.02 \
  --overflow-spill-penalty 0.01 \
  --volume-grid-points 101 \
  --turbine-flow-steps 8 \
  --pump-flow-steps 6
```

## Doxygen

```bash
cmake -S . -B build -DOPTIFLOW_BUILD_DOCS=ON
cmake --build build --target optiflow_docs
```

Generated HTML documentation is written under `build/docs/html`.

## Suggested first commit

```bash
git checkout -b restart/deterministic-hydro-core
git add .
git commit -m "Restart deterministic hydro optimization core"
```
