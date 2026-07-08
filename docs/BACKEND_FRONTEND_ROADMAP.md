# Backend and frontend roadmap

The repository now contains a runnable local demo path. The remaining improvements should be done in this order.

## 1. JSON scenario mapping

Current state: the API accepts a request body but the optimizer service runs the built-in sample scenario.

Next step:

- Add request validation.
- Map JSON input to `ModelParameters`, `State`, and `std::vector<Exogenous>`.
- Return validation errors for bad units, missing time points, negative capacities, and inconsistent horizons.

Keep the mapper outside `libs/optimization`.

## 2. PostgreSQL persistence

Current state: PostgreSQL schema is included and initialized in Docker Compose, but the API stores only the latest result in memory.

Next step:

- Add `ScenarioRepository`.
- Add `OptimizationRunRepository`.
- Persist scenario metadata and time series.
- Persist optimization runs and dispatch steps.

Use the schema in `db/migrations/001_init.sql`.

## 3. Real gRPC optimizer service

Current state: the optimizer boundary is a lightweight HTTP endpoint on port `50051`.

Next step:

- Generate C++ stubs from `protos/optiflow/v1/optimizer.proto`.
- Replace `/v1/optimize` with `Optimizer::Optimize`.
- Keep proto-to-core mapping in a service adapter.
- Keep `libs/optimization` protobuf-free.

## 4. Frontend improvements

Current state: static HTML/CSS/JavaScript with canvas charts.

Next step:

- Add scenario upload or editable inputs.
- Add run history.
- Add chart toggles for turbine, pump, charge, discharge, reservoir, SOC, net power, and cumulative profit.
- Add validation messages when actions are infeasible.

## 5. Numerical improvements

- Add action generation conditional on current state.
- Add greedy forward simulation using `V[t + 1]` rather than nearest-grid policy lookup.
- Add ramp limits.
- Add nonlinear efficiency curves.
- Add stochastic Markov regimes.
- Add performance benchmarks.
