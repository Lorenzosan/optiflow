# OptiFlow backend

This is a thin FastAPI service for local demo and interview discussion around HTTP APIs, web servers, Docker, and future ORM-backed run tracking.

The backend does not own the optimizer. The C++ optimizer remains in `libs/optimization` and the CLI remains the stable execution boundary. This first backend slice only exposes scenario discovery and a health check. Optimization execution, ORM persistence, PostgreSQL, NGINX, and frontend integration are intentionally left for later commits.

## Local Python run

```bash
python3 -m venv .venv
. .venv/bin/activate
pip install -r backend/requirements.txt
uvicorn backend.app.main:app --reload
```

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
```

## Docker run

From the repository root:

```bash
docker compose up --build api
```

Then check:

```bash
curl http://localhost:8000/health
curl http://localhost:8000/scenarios
```

The Docker image copies the example CSV inputs into `/app/examples` and sets `OPTIFLOW_REPO_ROOT=/app`, so `/scenarios` can verify that the referenced files are present inside the container.

## Intended next steps

1. Add SQLAlchemy models and PostgreSQL for scenarios, optimization runs, and run summaries.
2. Add a synchronous `POST /runs` endpoint that invokes the built `optiflow_solve` executable and stores run metadata.
3. Add dispatch CSV download and summary endpoints.
4. Add NGINX as a reverse proxy/load balancer in Docker Compose.
5. Add a minimal frontend consuming the HTTP API.
