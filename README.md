# OptiFlow

OptiFlow is a C++20 interview project for deterministic dispatch optimization of a pumped-storage asset with an optional battery.

## What is implemented

* Explicit scenario loading from separate CSV files.
* No default model or solver parameters.
* 2D state grid: reservoir volume x battery state of charge.
* Uniform action grid generated from explicit limits and explicit step counts.
* Pumped-storage and battery transition model.
* Deterministic Bellman dynamic-programming solver.
* Bilinear interpolation of the continuation value.
* Terminal-state hard bounds and soft target penalties.
* Value-function-based forward simulation.
* Nearest-policy forward simulation for grid-aligned verification.
* CSV dispatch output.
* Doxygen comments on public interfaces.
* Optimization runner facade used by the CLI.
* Runner-level optimization diagnostics shared by CLI and future service adapters.
* CLI diagnostic summary printed to stdout while keeping dispatch CSV trajectory-only.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Tests

The normal quality gate is the clean Release build plus CTest command shown above. It intentionally exercises only the local optimizer, CLI, CSV tools, and Python analysis scripts. There is no active gRPC/protobuf test gate.

The CTest suite is split by responsibility:

* `optiflow_tests` covers the core model and infrastructure: transition/reward accounting, invalid actions, bounds checks, state-grid lookup, value-function interpolation, terminal targets, CSV parsing failures, and `OptimizationRunner` diagnostics.
* `optiflow_simple_behaviour_tests` covers short hand-checkable dispatch behavior: wait at zero price, turbine at high price, pumping or charging when terminal inventory requires it, and failure reporting when terminal bounds are unreachable.
* `optiflow_optimizer_oracle_tests` covers deterministic economic oracles: turbine before a lower future price, preserve water before a higher future price, pump at negative price when feasible, use a low-degradation battery for arbitrage, avoid high-degradation battery cycling, and preserve reservoir inventory under a hard terminal constraint.
* `optiflow_solve_cli_example` checks that the CLI can solve the small CSV example and write a trajectory-only dispatch CSV while printing diagnostics to stdout.
* `optiflow_solve_cli_invalid_input` checks that malformed scenario input fails deterministically, reports the parse error on stderr, prints usage text, and does not create a dispatch CSV.
* `optiflow_solve_cli_yearly_example` checks the yearly synthetic scenario end to end, including terminal inventory bands and diagnostic counters.
* `optiflow_yearly_dispatch_summary` checks the summary tool against a generated yearly dispatch and verifies recomputed economic and energy-flow fields.
* `optiflow_dispatch_validator_regression` checks that the dispatch validator accepts a generated dispatch with CLI diagnostics and rejects a deliberately corrupted dispatch with a precise model-equation error.
* `optiflow_yearly_scenario_comparison` checks the comparison tool across the base, no-battery, and high-degradation yearly scenarios. It verifies that the physically unavailable battery case and the economically unattractive battery case are distinct inputs but produce the expected equal-profit comparison in the synthetic setup.

The C++ oracle tests are deliberately small and deterministic. They are meant to catch optimizer regressions before larger yearly examples or future service adapters are considered.

## Backend demo

The backend slice is a thin FastAPI service under `backend/`. It is intentionally an orchestration layer, not a replacement for the C++ optimizer core. The current endpoints are:

* `GET /health` for container and reverse-proxy health checks.
* `GET /scenarios` for discovering the bundled yearly scenarios.
* `POST /runs` for synchronously launching an optimization through the C++ CLI.
* `GET /runs/{run_id}` for retrieving persisted run status and dispatch artifact path.
* `GET /runs/{run_id}/dispatch.csv` for downloading a succeeded run's guarded CSV artifact.

Run it locally through Docker from the repository root:

```bash
docker compose up --build api
```

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl http://localhost:8000/runs/1
curl -OJ http://localhost:8000/runs/1/dispatch.csv
```

The Docker path builds the C++ CLI inside the API image, starts PostgreSQL, and stores scenario and run metadata through SQLAlchemy models. The backend seeds the bundled yearly scenarios at startup, verifies that referenced CSV files are present in `/scenarios`, and writes run dispatch artifacts under `build/api-runs`. NGINX load balancing and the frontend are intentionally left for follow-up commits.

## Run the sample

```bash
./build/apps/solve_cli/optiflow_solve \
  --scenario examples/scenario.csv \
  --prices examples/prices.csv \
  --inflows examples/inflows.csv \
  --output build/dispatch.csv
```

The repository also includes a deterministic one-year synthetic example with 8760 hourly time steps:

```bash
./build/apps/solve_cli/optiflow_solve \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --output build/yearly_dispatch.csv
```

The one-year example is intentionally coarse: it uses 9 reservoir grid points, 5 battery grid points, and 72 generated actions. It is useful for long-horizon smoke testing and diagnostics, not for calibrated economic conclusions.

The generated dispatch can be checked against the model equations and input files with the validation helper:

```bash
./build/apps/solve_cli/optiflow_solve \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --output build/yearly_dispatch.csv > build/yearly_stdout.txt

python3 tools/validate_dispatch.py \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --dispatch build/yearly_dispatch.csv \
  --stdout build/yearly_stdout.txt
```

The validator checks row counts, time indexing, state continuity, action-grid membership, physical bounds, mutual-exclusion constraints, transition equations, net power, reward, terminal hard bounds, and diagnostic activity counters. It does not prove that the one-year policy is globally optimal; that is covered by Bellman-solver tests on smaller scenarios where expected behavior is easier to assert.

The yearly dispatch can also be summarized into economic and energy-flow components:

```bash
python3 tools/summarize_dispatch.py \
  --scenario examples/yearly/scenario.csv \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --dispatch build/yearly_dispatch.csv
```

The summary reports export revenue, import cost, net market cashflow, operating cost, battery degradation cost, recomputed reward, energy imports and exports, weighted average operating prices, action counts, and final inventory relative to the configured terminal targets. It is an explanation tool, not a substitute for validation.

The yearly scenarios can also be compared in one command. The comparison tool runs the CLI for each scenario, writes one dispatch file per scenario, and emits a compact CSV table for profit, energy, inventory, action counts, and solver diagnostics:

```bash
python3 tools/compare_scenarios.py \
  --solve ./build/apps/solve_cli/optiflow_solve \
  --prices examples/yearly/prices.csv \
  --inflows examples/yearly/inflows.csv \
  --output-dir build/yearly-comparison \
  --scenario examples/yearly/scenario.csv \
  --scenario examples/yearly/scenario_no_battery.csv \
  --scenario examples/yearly/scenario_high_battery_degradation.csv \
  --summary-output build/yearly-comparison.csv
```

The included `scenario_no_battery.csv` uses the same yearly prices and inflows but sets the battery state range and battery power limits to zero. The included `scenario_high_battery_degradation.csv` keeps the battery physically available but assigns a high throughput cost, making battery cycling economically unattractive. The pair is intended to separate physical battery availability from economic battery use under the synthetic yearly assumptions.

The output file contains the dispatch trajectory with state, action, net power, reward, and cumulative profit. The CSV schema is trajectory-only and does not include run metadata.

The CLI prints a diagnostic summary to stdout after writing the dispatch CSV:

```text
Scenario: sample_day
Time steps: 12
Reservoir grid points: 21
Battery grid points: 11
Action count: 216
Solve seconds: <wall-clock seconds>
Simulation seconds: <wall-clock seconds>
Turbine steps: <count>
Pump steps: <count>
Spill steps: <count>
Battery charge steps: <count>
Battery discharge steps: <count>
Wait steps: <count>
Cumulative profit: <value>
Dispatch written to: build/dispatch.csv
```

The optimizer runner owns these diagnostics so future service adapters can expose the same metadata without recomputing it.

## Model conventions

The model units and sign conventions are documented in [`docs/model.md`](docs/model.md).

## Service boundary

The optimizer core is intentionally transport-independent. The current repository keeps the production path local and deterministic: CSV inputs are parsed by the CLI, and both tests and tools exercise the `OptimizationRunner` facade directly.

The previous protobuf/gRPC adapter has been removed from the active build while the project focuses on optimizer behavior, diagnostics, yearly simulation, and validation. A service adapter can be reintroduced later once the protobuf/gRPC toolchain is pinned and covered by a live serialization/integration test.

## Generate documentation

Doxygen is configured but not required for the build.

```bash
doxygen Doxyfile
```

Generated HTML documentation will be written under `docs/html`.

## Input files

The scenario CSV must contain all explicit scalar parameters:

```csv
key,value
```

The price CSV must contain:

```csv
time_index,price
```

The inflow CSV must contain:

```csv
time_index,natural_inflow
```

The price and inflow files must have matching `time_index` values, starting at zero and increasing by one. Every required scenario key must be present. Missing keys cause the CLI to fail. This is intentional.

The `examples/yearly/` directory follows the same schema and contains 8760 data rows in both `prices.csv` and `inflows.csv`, representing one non-leap year of hourly synthetic inputs.

Terminal-state behavior is controlled by explicit keys in the scenario file:

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

1. Add more input-validation regression tests for malformed or inconsistent scenario files.
2. Add deterministic sensitivity tests for terminal penalties, grid resolution, and action-grid resolution.
3. Add a small benchmark or timing smoke test for yearly-scale solves, keeping it separate from correctness assertions.
4. Reintroduce a service adapter only after the optimizer core remains stable and the serialization/toolchain path is pinned and tested end to end.
