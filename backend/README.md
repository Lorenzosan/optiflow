# OptiFlow backend

This is a thin FastAPI service for local demo and interview discussion around HTTP APIs, web servers, Docker, ORM-backed persistence, and future run tracking.

The backend does not own the optimizer. The C++ optimizer remains in `libs/optimization` and the CLI remains the stable execution boundary. This backend slice exposes scenario discovery from a SQLAlchemy-managed database, a health check, synchronous optimization run execution through the C++ CLI, persisted run summaries, and guarded dispatch CSV download. NGINX and frontend integration are intentionally left for later commits.

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
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl http://localhost:8000/runs/1
curl -OJ http://localhost:8000/runs/1/dispatch.csv
```

## Docker run

From the repository root:

```bash
docker compose up --build api
```

This builds the C++ solver inside the API image, then starts both the API and PostgreSQL. The API waits for PostgreSQL to pass its health check, applies Alembic migrations, and seeds the three included yearly scenarios.

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl http://localhost:8000/runs/1
curl -OJ http://localhost:8000/runs/1/dispatch.csv
```

The Docker image copies the example CSV inputs into `/app/examples`, builds `/app/build/apps/solve_cli/optiflow_solve`, and sets `OPTIFLOW_REPO_ROOT=/app`, so `/scenarios` can verify that the referenced files are present inside the container. Optimization runs write dispatch artifacts under `/app/build/api-runs`, backed by a Docker volume.

## Database model

The ORM stores durable control-plane data and compact run results:

* `Scenario`: scenario name, description, and paths to scenario, price, and inflow CSV files.
* `OptimizationRun`: run-tracking table with scenario foreign key, status, timestamps, output dispatch path, and error message.
* `RunSummary`: one-to-one summary with profit, energy, terminal inventory, timing, and action counters for succeeded runs.

The dispatch trajectory stays as a CSV artifact. It is not expanded into relational rows in this phase. Database schema changes are versioned under `backend/alembic/`; application startup no longer calls `create_all`.

## One-time migration transition

Earlier backend commits created tables directly with SQLAlchemy and did not write an Alembic revision marker. Choose one transition path before starting this version.

For disposable local data, reset the old databases. The Docker command also deletes the dispatch-artifact volume:

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

1. Add `GET /runs` with bounded pagination and stable ordering.
2. Add NGINX as a reverse proxy/load balancer in Docker Compose.
3. Add a minimal frontend consuming the HTTP API.
