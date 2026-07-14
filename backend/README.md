# OptiFlow backend

The backend is a thin FastAPI orchestration layer around the C++ pumped-storage optimizer. It provides scenario discovery and validated immutable uploads, synchronous run execution, persisted run summaries and provenance, history queries, and guarded dispatch downloads.

The optimizer remains transport-independent under `libs/optimization`; the CLI is the service execution boundary.

## Local run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -r backend/requirements-dev.txt
python -m alembic upgrade head
python -m pytest -q backend/tests
uvicorn backend.app.main:app --reload
```

## Docker

```bash
docker compose up --build
```

The stack starts PostgreSQL, applies Alembic migrations, seeds the yearly hydro scenarios, starts the API, and serves the frontend through NGINX.

## API

* `GET /health`
* `GET /scenarios`
* `POST /scenarios` with multipart `description`, `scenario`, `prices`, and `inflows`
* `POST /runs`
* `GET /runs` with bounded pagination and optional scenario/status filters
* `GET /runs/{run_id}`
* `GET /runs/{run_id}/dispatch.csv`

Uploaded inputs are staged, validated through the C++ CLI with `--validate-only`, and moved to an immutable server-generated directory only after successful validation.

## Persistence

* `Scenario`: metadata and immutable input paths.
* `OptimizationRun`: status, timestamps, artifact path, and errors.
* `RunSummary`: profit, import/export energy, final reservoir inventory, timings, and hydraulic action counters.
* `RunProvenance`: SHA-256 hashes for the three inputs, solver executable, and successful dispatch; horizon and grid configuration; and the result-schema version.

Database timestamp columns store naive values with an explicit UTC contract for compatibility with the existing PostgreSQL schema. The API serializes run timestamps as timezone-aware UTC values ending in `Z`.

Existing runs created before the provenance migration remain valid and return `provenance: null`. New failed runs retain input and solver provenance when those execution inputs were readable; only successful runs have a dispatch hash. Dispatch trajectories remain CSV artifacts rather than relational rows.

## Migration

Run:

```bash
python -m alembic upgrade head
python -m alembic check
```

The migration chain creates the current reservoir-only schema directly.
