# OptiFlow

OptiFlow is a deterministic C++20 dynamic-programming demo for pumped-storage hydro dispatch over a finite price and inflow horizon.

This restart intentionally focuses on the deterministic model core. It does not include stochastic optimization, frontend code, services, PostgreSQL, gRPC, or Docker integration yet.

## Current scope

- Read deterministic price and inflow CSV files.
- Validate physical and numerical model constraints.
- Build a reservoir state grid and a hydro action grid.
- Add reachable-state and terminal-target anchors to the reservoir grid.
- Solve a deterministic Bellman recursion with a final-reservoir target penalty.
- Simulate a forward rollout from the initial reservoir volume using the solved Bellman value functions.
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

## Solver boundary

The solver does not read files or parse CLI arguments. CSV and CLI code build a validated in-memory `DeterministicProblem`:

```text
DeterministicProblem =
  exogenous price/inflow series
  reservoir constraints
  hydro physics
  economic parameters
  terminal reservoir target
  solver discretization config
```

`BellmanSolver::solve()` accepts this problem object directly. This keeps the optimization core independent from CSV, JSON, HTTP, databases, and future frontend/API DTOs.

The state grid starts from the requested uniform resolution and then inserts important physical states, including the initial reservoir volume, target final reservoir volume, and reservoir volumes reachable from the initial condition under the configured action grid. This keeps the reported Bellman objective consistent with the forward rollout for the deterministic CLI case.

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
  --target-final-reservoir-volume 500000 \
  --terminal-reservoir-penalty 0.10 \
  --overflow-spill-penalty 0.01 \
  --volume-grid-points 101 \
  --turbine-flow-steps 8 \
  --pump-flow-steps 6
```


## Terminal target penalty

This restart does not assign an economic terminal water value. Instead, the optimizer can be given a target final reservoir volume and a linear penalty for ending above or below that target:

```text
terminal_penalty_eur = terminal_reservoir_penalty_eur_per_m3
                       * abs(final_reservoir_volume_m3 - target_final_reservoir_volume_m3)
```

This keeps the model simple while discouraging artificial end-of-horizon draining. The default target is the initial reservoir volume.

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
