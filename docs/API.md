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

Runs the optimizer for the posted scenario. In the current dependency-free demo, the optimizer service runs the built-in sample scenario. The request body is accepted to preserve the intended REST shape, but full JSON-to-C++ scenario mapping is a deliberate next step.

## `GET /api/runs/latest`

Returns the last optimization result held in API service memory. PostgreSQL schema support is included under `db/migrations`; runtime persistence is the next backend milestone.
