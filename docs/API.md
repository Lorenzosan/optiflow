# API surface

The local demo API intentionally exposes a small workflow.

## `GET /api/health`

Returns API service liveness.

```json
{"service":"api","status":"ok"}
```

## `GET /api/scenarios/sample`

Returns the sample day-ahead price and inflow scenario used by the demo dashboard.

## `POST /api/optimizations`

Runs the optimizer for the posted scenario. The API service forwards the request body to the optimizer service. The optimizer service parses the lightweight local JSON shape, validates the exogenous horizon, runs the Bellman solver, and returns the forward-simulated dispatch result.

Empty request bodies and `{}` are kept as a backwards-compatible shortcut for the built-in deterministic sample scenario.

Deterministic request:

```json
{
  "exogenous": [
    {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0},
    {"time_index": 1, "price_eur_per_mwh": -10.0, "natural_inflow_m3_s": 2.5}
  ]
}
```

Validation currently covers:

- `exogenous` arrays with contiguous ordered `time_index` values starting at zero.
- finite price and inflow values.

Current limitation: the lightweight JSON request mapper uses the default model parameters, initial state, state grid, and action grid. It maps exogenous data first. Full JSON-to-domain mapping for model parameters and grids remains a later service-layer step.

## `GET /api/runs/latest`

Returns the last optimization result held in API service memory. PostgreSQL schema support is included under `db/migrations`; runtime persistence is the next backend milestone.
