# OptiFlow backend

The backend is a thin FastAPI orchestration layer around the C++ pumped-storage optimizer. It provides scenario discovery and validated managed uploads, synchronous run execution, persisted run summaries, history queries, and guarded dispatch downloads.

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
* `POST /scenarios` with multipart `description`, `scenario`, `prices`, `inflows`, and optional `overwrite`
* `POST /runs`
* `GET /runs` with bounded pagination and optional scenario/status filters
* `GET /runs/{run_id}`
* `GET /runs/{run_id}/dispatch.csv`

Uploaded inputs are staged, validated through the C++ CLI with `--validate-only`, and moved to a server-generated directory only after successful validation. With `overwrite=true`, an existing custom scenario with the same name is replaced in place and its prior runs, summaries, dispatch artifacts, and previous input directory are deleted. Bundled scenarios cannot be overwritten.

## Persistence

* `Scenario`: metadata and current managed input paths.
* `OptimizationRun`: status, timestamps, artifact path, and errors.
* `RunSummary`: profit, import/export energy, final reservoir inventory, timings, and hydraulic action counters.

Database timestamp columns store naive values with an explicit UTC contract for compatibility with the existing PostgreSQL schema. The API serializes run timestamps as timezone-aware UTC values ending in `Z`.

Dispatch trajectories remain CSV artifacts rather than relational rows.

## Migration

Run:

```bash
python -m alembic upgrade head
python -m alembic check
```

The migration chain creates the current schema through revision `20260710_0002`. This development repository assumes a fresh database after migration-history changes; recreate the database instead of preserving obsolete revisions.
