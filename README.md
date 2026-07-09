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
* Optional protobuf/gRPC optimizer contract generation.
* Protobuf conversion layer between `OptimizeRequest` and `ScenarioBundle`.
* Local gRPC optimizer service.
* gRPC service smoke test.
* Runner-level optimization diagnostics shared by CLI and gRPC boundaries.

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

The output file contains the dispatch trajectory with state, action, net power, reward, and cumulative profit.

The optimizer runner also reports diagnostics to API callers. These diagnostics include horizon length, state-grid dimensions, action-grid size, Bellman solve time, forward-simulation time, and counts of simulated turbine, pump, spill, battery charge, battery discharge, and wait steps. The CLI currently keeps the CSV output trajectory-only; service clients receive diagnostics through `OptimizeResponse.diagnostics`.

## Model conventions

The model units and sign conventions are documented in [`docs/model.md`](docs/model.md).

## Build protobuf/gRPC targets

The protobuf/gRPC boundary is optional and is disabled by default so the optimizer core can still build without protobuf or gRPC installed. Enable it explicitly when protobuf and gRPC are available:

```bash
cmake -S . -B build-grpc -DCMAKE_BUILD_TYPE=Release -DOPTIFLOW_BUILD_GRPC=ON
cmake --build build-grpc -j
ctest --test-dir build-grpc --output-on-failure
```

Generated protobuf and gRPC C++ files are written under the build directory and should not be committed.

To build only the generated protobuf/gRPC target:

```bash
cmake --build build-grpc -j --target optiflow_optimizer_proto
```

## Run the local gRPC service

After building with `OPTIFLOW_BUILD_GRPC=ON`, run:

```bash
./build-grpc/apps/optimizer_service/optiflow_optimizer_service_server \
  --address 127.0.0.1:50051
```

The local service exposes:

```text
OptimizerService.Optimize(OptimizeRequest) -> OptimizeResponse
```

The service maps typed protobuf requests to the internal optimization runner. CSV parsing remains a CLI concern.

`OptimizeResponse.diagnostics` is copied from `OptimizationRunner`; the protobuf conversion layer does not recompute solver metadata. The gRPC smoke tests check that structural diagnostics and dispatch-derived activity counters survive the service boundary.

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

1. Add a small local gRPC client for manual `OptimizeRequest` calls.
2. Add a documented example request builder for the gRPC API.
3. Add CLI diagnostic printing without changing the dispatch CSV schema.
4. Add protobuf/CSV optimization parity tests.
5. Add persistence schema and API service only after the local optimizer service interface is stable.
6. Add Docker Compose and frontend last.
