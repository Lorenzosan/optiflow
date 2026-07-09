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
  --summary-output build/yearly-comparison.csv
```

The included `scenario_no_battery.csv` uses the same yearly prices and inflows but sets the battery state range and battery power limits to zero. It is intended to isolate the marginal value of the battery under the synthetic yearly assumptions.

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

1. Add additional deterministic scenarios: no battery, high inflow, low inflow, and negative-price periods.
2. Add small oracle tests for Bellman-solver decisions on hand-checkable horizons.
3. Add scenario-comparison reporting so multiple CSV scenarios can be compared consistently.
4. Reintroduce a service adapter only after the serialization/toolchain path is pinned and tested end to end.
