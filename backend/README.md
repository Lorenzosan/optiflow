# OptiFlow backend

This is a thin FastAPI service for local demo and interview discussion around HTTP APIs, Docker, ORM-backed persistence, and optimization run tracking.

The backend does not own the optimizer. The C++ optimizer remains in `libs/optimization` and the CLI remains the stable execution boundary. This backend slice exposes scenario discovery and immutable custom-scenario uploads from a SQLAlchemy-managed database, a health check, synchronous optimization run execution through the C++ CLI, persisted run summaries, and guarded dispatch CSV download. The static frontend under `frontend/` reaches these endpoints through an NGINX `/api/` reverse proxy.

`GET /runs` returns `{items, total, limit, offset}` with newest-first ordering, bounded `limit`/`offset` pagination, and optional `scenario_id` and `status` filters.

## Local Python run

The backend defaults to a local SQLite database when `OPTIFLOW_DATABASE_URL` is not set. This is useful for quick development without Docker. Local optimization execution also requires the C++ CLI to be built at `build/apps/solve_cli/optiflow_solve`.

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

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
curl -X POST http://localhost:8000/scenarios \
  -F 'description=Uploaded scenario' \
  -F 'scenario=@examples/scenario.csv;type=text/csv' \
  -F 'prices=@examples/prices.csv;type=text/csv' \
  -F 'inflows=@examples/inflows.csv;type=text/csv'
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl 'http://localhost:8000/runs?limit=20&offset=0'
curl http://localhost:8000/runs/1
curl -OJ http://localhost:8000/runs/1/dispatch.csv
```

## Docker run

From the repository root:

```bash
docker compose up --build
```

This builds the C++ solver and static frontend, then starts NGINX, the API, and PostgreSQL. The API applies Alembic migrations and seeds the three included yearly scenarios before NGINX is marked ready.

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
curl -X POST http://localhost:8000/scenarios \
  -F 'description=Uploaded scenario' \
  -F 'scenario=@examples/scenario.csv;type=text/csv' \
  -F 'prices=@examples/prices.csv;type=text/csv' \
  -F 'inflows=@examples/inflows.csv;type=text/csv'
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl 'http://localhost:8000/runs?limit=20&offset=0'
curl http://localhost:8000/runs/1
curl -OJ http://localhost:8000/runs/1/dispatch.csv
```

The Docker image copies the example CSV inputs into `/app/examples`, builds `/app/build/apps/solve_cli/optiflow_solve`, and sets `OPTIFLOW_REPO_ROOT=/app`. Uploaded inputs are stored under `/app/data/scenarios` in a dedicated Docker volume, while optimization runs write dispatch artifacts under `/app/build/api-runs` in a separate volume.

## Custom scenario uploads

`POST /scenarios` accepts multipart fields `description`, `scenario`, `prices`, and `inflows`. All three files must use a `.csv` filename. The scenario file is limited to 256 KiB; each time-series file is limited to 8 MiB. The API stores files under server-generated immutable directories and derives the database name from the validated `scenario_name` entry. Duplicate scenario names return HTTP 409.

Before a database row is created, the API runs the C++ CLI with `--validate-only`. This applies the same CSV parsing, model bounds, terminal constraints, initial-state checks, and solver-grid validation used by real optimization runs. Invalid inputs return HTTP 422 and their staged files are removed.

Uploaded scenario files are not edited in place. A future editor should clone an existing scenario and save a new name so earlier runs remain reproducible.

## Database model

The ORM stores durable control-plane data and compact run results:

* `Scenario`: scenario name, description, and paths to scenario, price, and inflow CSV files.
* `OptimizationRun`: run-tracking table with scenario foreign key, status, timestamps, output dispatch path, and error message.
* `RunSummary`: one-to-one summary with profit, energy, terminal inventory, timing, and action counters for succeeded runs.

The dispatch trajectory stays as a CSV artifact. It is not expanded into relational rows in this phase. Database schema changes are versioned under `backend/alembic/`; application startup no longer calls `create_all`.

## One-time migration transition

Earlier backend commits created tables directly with SQLAlchemy and did not write an Alembic revision marker. Choose one transition path before starting this version.

For disposable local data, reset the old databases. The Docker command also deletes the dispatch-artifact and uploaded-scenario volumes:

```bash
rm -f optiflow_api.db
docker compose down -v
```

To preserve a pre-Alembic database containing the original `scenarios` and `optimization_runs` tables, mark only the initial schema as applied, then run later migrations normally:

```bash
python -m alembic stamp 20260710_0001
python -m alembic upgrade head
python -m alembic check
```

For the Docker PostgreSQL database, build the new image and run the same adoption commands through the API service:

```bash
docker compose build api
docker compose run --rm api python -m alembic stamp 20260710_0001
docker compose run --rm api python -m alembic upgrade head
docker compose run --rm api python -m alembic check
```

After either transition, `python -m alembic upgrade head` initializes or upgrades local SQLite, while the Docker API command upgrades PostgreSQL automatically.

## Intended next steps

1. Add a lightweight custom-scenario form that generates `scenario.csv` and uploads price/inflow CSV files.
2. Add dispatch visualization only after the tabular workflow remains stable.
3. Consider asynchronous execution only when multi-user or long-running deployment requirements justify it.
