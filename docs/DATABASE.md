# Database schema

The PostgreSQL schema is under `db/migrations/001_init.sql`.

The intended persistence model is:

- `scenarios`: scenario metadata and initial state.
- `scenario_time_series`: price and inflow inputs by time index.
- `optimization_runs`: one row per optimization request.
- `dispatch_steps`: simulated policy application for each time step.

The local C++ API service currently keeps the latest result in memory to avoid adding a libpq/libpqxx dependency to the default build. The Docker Compose stack still starts PostgreSQL and applies the schema so that persistence can be added without changing the deployment topology.

Recommended next backend increment:

1. Add a `ScenarioRepository` backed by PostgreSQL.
2. Add an `OptimizationRunRepository` for run metadata and dispatch rows.
3. Keep repository classes outside `libs/optimization`.
4. Store optimizer inputs and outputs at API boundaries, not inside the numerical solver.
