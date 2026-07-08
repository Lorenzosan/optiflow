# OptiFlow

OptiFlow is a C++20 interview project for flexible energy-storage dispatch optimization. It models a pumped-storage asset with an optional battery, solves a finite-horizon dispatch problem with Bellman dynamic programming, and exposes the result through a local backend/frontend demo scaffold.

The project is deliberately split into a dependency-light numerical core and optional service/deployment layers.

## What it demonstrates

- C++20 value-type domain modeling.
- Deterministic Bellman dynamic programming over a two-dimensional state grid.
- Bilinear interpolation of continuation values.
- Forward simulation of the computed policy.
- A first stochastic dynamic-programming extension.
- Lightweight C++ API and optimizer services for a local demo path.
- Docker Compose deployment with NGINX, API, optimizer, and PostgreSQL.
- Doxygen-ready public headers.

## Repository structure

```text
apps/optiflow_cli/                 CLI smoke-test runner
libs/optimization/                 Numerical optimization library
libs/demo/                         Shared sample scenario and JSON/CSV formatting
libs/service_common/               Minimal dependency-free HTTP utilities
services/api_service/              Lightweight C++ API service
services/optimizer_service/        Lightweight C++ optimizer service
frontend/                          Static browser dashboard
infra/nginx/                       NGINX reverse-proxy config
db/migrations/                     PostgreSQL schema
protos/optiflow/v1/                Intended gRPC service contract
docs/                              Architecture, model, API, deployment, stochastic notes
tests/optimization_scenarios/      Scenario-level tests
samples/                           Example input data
```

## Build the numerical core

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/apps/optiflow_cli/optiflow_cli
```

## Build the local C++ services

```bash
cmake -S . -B build/services \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOPTIFLOW_BUILD_SERVICES=ON
cmake --build build/services -j
```

Run in separate terminals:

```bash
OPTIFLOW_OPTIMIZER_PORT=50051 ./build/services/services/optimizer_service/optiflow_optimizer_service
```

```bash
OPTIFLOW_API_PORT=8080 \
OPTIFLOW_OPTIMIZER_URL=http://127.0.0.1:50051/v1/optimize \
./build/services/services/api_service/optiflow_api_service
```

Then:

```bash
curl http://127.0.0.1:8080/api/scenarios/sample
curl -X POST http://127.0.0.1:8080/api/optimizations -H 'Content-Type: application/json' -d @samples/optimization_request_deterministic.json
```

## Run the local full-stack demo

```bash
docker compose up --build
```

Open:

```text
http://localhost:8080
```

The browser dashboard loads a sample price/inflow scenario, calls the C++ API, runs the optimizer service, and plots dispatch results.


## Scenario inputs and constraints

The local demo accepts separated price and inflow inputs. Deterministic CSV files use one row per time step:

```csv
# prices
time_index,price_eur_per_mwh
0,15.0
1,-20.0
```

```csv
# inflows
time_index,natural_inflow_m3_s
0,0.0
1,5.0
```

Stochastic CSV files use paired realizations. The same `(time_index, realization_index)` in the price and inflow files is one joint outcome, so price and inflow correlation is preserved:

```csv
# stochastic prices
time_index,realization_index,probability,price_eur_per_mwh
0,0,0.5,15.0
0,1,0.5,45.0
```

```csv
# stochastic inflows
time_index,realization_index,probability,natural_inflow_m3_s
0,0,0.5,0.0
0,1,0.5,10.0
```

The browser dashboard includes a model-constraints panel. The values in that panel are sent with every optimization request and are used by the optimizer service. The important request fields are:

```json
{
  "solver_kind": "deterministic",
  "initial_state": {
    "reservoir_volume_m3": 50000000.0,
    "battery_soc_mwh": 25.0
  },
  "parameters": {
    "timestep_hours": 1.0,
    "terminal_water_value_eur_per_m3": 0.001,
    "terminal_battery_value_eur_per_mwh": 5.0,
    "hydro": {
      "min_reservoir_volume_m3": 0.0,
      "max_reservoir_volume_m3": 100000000.0,
      "max_turbine_flow_m3_s": 150.0,
      "max_pump_flow_m3_s": 75.0,
      "max_spill_flow_m3_s": 260.0,
      "hydraulic_head_m": 120.0,
      "turbine_efficiency": 0.90,
      "pump_efficiency": 0.85,
      "turbine_cost_eur_per_mwh": 1.0,
      "pump_cost_eur_per_mwh": 0.5,
      "spill_penalty_eur_per_m3": 0.0
    },
    "battery": {
      "enabled": true,
      "capacity_mwh": 50.0,
      "max_charge_mw": 25.0,
      "max_discharge_mw": 25.0,
      "charge_efficiency": 0.95,
      "discharge_efficiency": 0.95,
      "degradation_cost_eur_per_mwh": 1.0
    }
  },
  "optimization_config": {
    "discount_factor": 1.0
  },
  "exogenous": []
}
```

The optimizer validates these constraints before solving. In particular, reservoir bounds must be ordered, the initial state must lie inside the configured bounds, inflows must be non-negative, efficiencies must be in `(0, 1]`, and the discount factor must be in `[0, 1]`.

The numerical grid is derived from the submitted constraints in the local demo path. The reservoir grid spans the configured min/max reservoir volume, the battery grid spans zero to configured capacity when the battery is enabled, and action-grid limits are derived from turbine, pump, spill, charge, and discharge limits. This keeps the frontend-visible constraints aligned with the optimization problem that is actually solved.

## Doxygen

```bash
cmake -S . -B build/docs \
  -DOPTIFLOW_BUILD_DOCS=ON \
  -DOPTIFLOW_BUILD_CLI=OFF \
  -DOPTIFLOW_BUILD_TESTS=OFF
cmake --build build/docs --target optiflow_docs
```

Generated HTML appears under the configured Doxygen output directory in the build tree.

## Current backend status

The repository includes a runnable dependency-free service path:

```text
browser -> nginx -> api service -> optimizer service -> optimization library
```

The optimizer service currently uses lightweight HTTP on port `50051`. The protobuf contract is included under `protos/` as the intended migration path to real gRPC once `grpc++` and `protobuf` are added to the build environment.

The lightweight optimizer request path accepts deterministic and stochastic exogenous data in JSON, selects the corresponding solver, and applies submitted model parameters, initial state, and optimization configuration. PostgreSQL is included in Docker Compose and the schema is applied automatically, but the API service currently keeps the latest optimization result in memory. Runtime PostgreSQL persistence is intentionally isolated as the next backend increment so that it does not pollute the numerical library.

## Mathematical model

State:

```text
reservoir volume x battery state of charge
```

Action:

```text
turbine flow, spill flow, pump flow, battery charge, battery discharge
```

Exogenous input:

```text
electricity price, natural inflow
```

Objective:

```text
market revenue - operating costs - degradation costs - penalties + terminal value
```

The deterministic recursion is:

```text
V_t(s) = max_a [ r_t(s, a) + gamma V_{t+1}(f_t(s, a)) ]
```

The stochastic extension is:

```text
V_t(s) = max_a E_w [ r(s, a, w) + gamma V_{t+1}(f(s, a, w)) ]
```

## Limits

This is a demo model, not a production hydro-bidding system. Current simplifications include deterministic default inputs, fixed hydraulic head, simplified battery degradation, discrete state/action grids, no ramp constraints, no market gate closure, no reserve products, no stochastic forward simulation, and no runtime database persistence yet.

Those limitations are explicit because they create a clean interview discussion path: first the numerical core, then service boundaries, then persistence, then stochastic and market-realism extensions.
