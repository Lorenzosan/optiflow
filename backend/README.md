# OptiFlow backend

The backend is a thin FastAPI orchestration layer around the C++ pumped-storage optimizer. It provides scenario discovery and validated managed uploads, synchronous run execution, persisted run summaries, history queries, and guarded dispatch downloads.

The optimizer remains transport-independent under `libs/optimization`; the CLI is the service execution boundary.

## Local run

Local backend development requires CMake 3.20 or newer, a C++20 compiler, and Python 3.12. PostgreSQL is optional for this path because the default database URL uses SQLite.

Build the solver, create a virtual environment, and install the bounded dependencies from `backend/requirements-dev.txt`:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
python3 -m venv .venv
```

Activate the environment on macOS or Linux:

```bash
. .venv/bin/activate
```

Or in Windows PowerShell:

```powershell
.\.venv\Scripts\Activate.ps1
```

Then run:

```bash
python -m pip install -r backend/requirements-dev.txt
python -m alembic upgrade head
python -m pytest -q backend/tests
python -m uvicorn backend.app.main:app --reload
```

The Docker workflow remains the canonical complete-application deployment because it also provides NGINX and PostgreSQL.

## Docker

```bash
docker compose up --build
```

The stack starts PostgreSQL, applies Alembic migrations, seeds the bundled multistep and yearly scenarios, starts the API, and serves the frontend through NGINX.

## API

* `GET /health`
* `GET /scenarios`
* `GET /scenarios/{scenario_id}/inputs` for guarded editor hydration
* `POST /scenarios` with multipart `scenario`, `prices`, `inflows`, optional `description`, and optional `overwrite`
* `POST /runs`
* `GET /runs` with bounded pagination and optional scenario/status filters
* `GET /runs/{run_id}`
* `GET /runs/{run_id}/dispatch.csv`

Stored scenario inputs can be read through the guarded scenario-input endpoint for browser editing; paths outside the repository or configured scenario storage are rejected. Uploaded inputs are staged, validated through the C++ CLI with `--validate-only`, and moved to a server-generated directory only after successful validation. With `overwrite=true`, an existing custom scenario with the same name is replaced in place and its prior runs, summaries, dispatch artifacts, and previous input directory are deleted. Bundled scenarios cannot be overwritten.

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
