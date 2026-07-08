# Local deployment

## Build the C++ core

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Build the lightweight services

```bash
cmake -S . -B build/services \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOPTIFLOW_BUILD_SERVICES=ON
cmake --build build/services -j
```

Run manually in separate terminals:

```bash
OPTIFLOW_OPTIMIZER_PORT=50051 ./build/services/services/optimizer_service/optiflow_optimizer_service
```

```bash
OPTIFLOW_API_PORT=8080 \
OPTIFLOW_OPTIMIZER_URL=http://127.0.0.1:50051/v1/optimize \
./build/services/services/api_service/optiflow_api_service
```

Then test:

```bash
curl http://127.0.0.1:8080/api/scenarios/sample
curl -X POST http://127.0.0.1:8080/api/optimizations -H 'Content-Type: application/json' -d @samples/optimization_request_deterministic.json
```

## Docker Compose

```bash
docker compose up --build
```

Open:

```text
http://localhost:8080
```

Services:

- `nginx`: static frontend and `/api` reverse proxy.
- `api`: lightweight C++ HTTP API service.
- `optimizer`: lightweight C++ optimizer service.
- `postgres`: schema-ready PostgreSQL database.

The optimizer service listens on port `50051` because that is the intended future gRPC port. The current transport is HTTP to avoid making gRPC a hard dependency in the default build.
