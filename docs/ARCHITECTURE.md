# Architecture

OptiFlow is organized around one rule: the numerical optimizer must not depend on transport, persistence, or UI details.

## Layers

```text
frontend
  static browser dashboard

nginx
  serves frontend and proxies /api

services/api_service
  REST-like HTTP boundary for scenario loading and optimization requests

services/optimizer_service
  optimizer boundary; currently lightweight HTTP, intended gRPC contract in protos/

libs/demo
  sample scenario construction and JSON/CSV formatting for the local demo

libs/optimization
  physical model, grids, Bellman solvers, policy, and simulator

db/migrations
  PostgreSQL schema for scenarios, runs, and dispatch steps
```

## Dependency direction

```text
frontend -> api_service -> optimizer_service -> optimization library
```

The optimization library has no knowledge of HTTP, gRPC, SQL, Docker, NGINX, or JSON.

## Why the service layer is lightweight

The local service path is intentionally dependency-free C++ HTTP. This makes the repository easy to compile on a clean machine while preserving the architecture that will later be replaced by production-grade libraries:

- HTTP demo transport -> gRPC using `protos/optiflow/v1/optimizer.proto`.
- In-memory latest run -> PostgreSQL repositories.
- Manual JSON formatting -> structured JSON/protobuf mappers.

This avoids the common interview-project failure mode where the first build requires a large dependency stack before the numerical work can even be inspected.

## Current runtime topology

```text
Browser
  |
  v
NGINX :8080
  |
  v
C++ API service :8080 inside compose network
  |
  v
C++ optimizer service :50051 inside compose network
  |
  v
Bellman solver and forward simulator
```

PostgreSQL is started and initialized by Docker Compose. Runtime repository integration is intentionally left as the next backend increment.
