# OptiFlow backend

This is a thin FastAPI service for local demo and interview discussion around HTTP APIs, web servers, Docker, ORM-backed persistence, and future run tracking.

The backend does not own the optimizer. The C++ optimizer remains in `libs/optimization` and the CLI remains the stable execution boundary. This backend slice exposes scenario discovery from a SQLAlchemy-managed database, a health check, and synchronous optimization run execution through the C++ CLI. Dispatch download, NGINX, and frontend integration are intentionally left for later commits.

## Local Python run

The backend defaults to a local SQLite database when `OPTIFLOW_DATABASE_URL` is not set. This is useful for quick development without Docker. Local optimization execution also requires the C++ CLI to be built at `build/apps/solve_cli/optiflow_solve`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

python3 -m venv .venv
. .venv/bin/activate
pip install -r backend/requirements.txt
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
```

## Docker run

From the repository root:

```bash
docker compose up --build api
```

This builds the C++ solver inside the API image, then starts both the API and PostgreSQL. The API waits for PostgreSQL to pass its health check, creates the minimal schema on startup, and seeds the three included yearly scenarios.

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
curl -X POST http://localhost:8000/runs \
  -H "Content-Type: application/json" \
  -d '{"scenario_id":1}'
curl http://localhost:8000/runs/1
```

The Docker image copies the example CSV inputs into `/app/examples`, builds `/app/build/apps/solve_cli/optiflow_solve`, and sets `OPTIFLOW_REPO_ROOT=/app`, so `/scenarios` can verify that the referenced files are present inside the container. Optimization runs write dispatch artifacts under `/app/build/api-runs`, backed by a Docker volume.

## Database model

The first ORM slice intentionally stores only durable control-plane data:

* `Scenario`: scenario name, description, and paths to scenario, price, and inflow CSV files.
* `OptimizationRun`: run-tracking table with scenario foreign key, status, timestamps, output dispatch path, and error message.

The dispatch trajectory stays as a CSV artifact. It is not expanded into relational rows in this phase.

## Intended next steps

1. Add dispatch CSV download and summary endpoints.
2. Add NGINX as a reverse proxy/load balancer in Docker Compose.
3. Add a minimal frontend consuming the HTTP API.
