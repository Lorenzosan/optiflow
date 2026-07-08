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

Runs the optimizer for the posted scenario. The API service forwards the request body to the optimizer service. The optimizer service parses the lightweight local JSON shape, validates the exogenous horizon, runs the selected solver, and returns the forward-simulated dispatch result.

Empty request bodies and `{}` are kept as a backwards-compatible shortcut for the built-in deterministic sample scenario.

Deterministic request:

```json
{
  "solver_kind": "deterministic",
  "exogenous": [
    {"time_index": 0, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0},
    {"time_index": 1, "price_eur_per_mwh": -10.0, "natural_inflow_m3_s": 2.5}
  ]
}
```

Stochastic request:

```json
{
  "solver_kind": "stochastic",
  "stochastic_process": [
    {
      "time_index": 0,
      "realizations": [
        {"realization_index": 0, "probability": 0.5, "price_eur_per_mwh": 20.0, "natural_inflow_m3_s": 0.0},
        {"realization_index": 1, "probability": 0.5, "price_eur_per_mwh": 60.0, "natural_inflow_m3_s": 5.0}
      ]
    }
  ]
}
```

Validation currently covers:

- `solver_kind` values: `deterministic`, `stochastic`, or `stochastic_stagewise`.
- deterministic `exogenous` arrays with contiguous ordered `time_index` values starting at zero.
- stochastic `stochastic_process` arrays with contiguous ordered `time_index` and `realization_index` values.
- finite price, inflow, and probability values.
- stochastic probabilities summing to `1.0` per stage.

Current limitation: the lightweight JSON request mapper uses the default model parameters, initial state, state grid, and action grid. It maps exogenous data and solver selection first. Full JSON-to-domain mapping for model parameters and grids remains a later service-layer step.

## `GET /api/runs/latest`

Returns the last optimization result held in API service memory. PostgreSQL schema support is included under `db/migrations`; runtime persistence is the next backend milestone.
