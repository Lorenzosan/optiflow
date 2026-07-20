#!/usr/bin/env sh
set -eu

repository_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$repository_root"

if ! command -v docker >/dev/null 2>&1; then
  cat >&2 <<'MESSAGE'
Docker is required to run OptiFlow.
Install Docker Desktop on macOS or Windows, or Docker Engine with the Compose plugin on Linux.
MESSAGE
  exit 1
fi

if ! docker compose version >/dev/null 2>&1; then
  cat >&2 <<'MESSAGE'
The Docker Compose plugin is required.
Install or update Docker Desktop, or install the Docker Compose plugin for Docker Engine.
MESSAGE
  exit 1
fi

if ! docker info >/dev/null 2>&1; then
  cat >&2 <<'MESSAGE'
The Docker daemon is not available.
Start Docker Desktop, or start the Docker service and verify that your user can access it.
MESSAGE
  exit 1
fi

: "${OPTIFLOW_WEB_PORT:=8080}"
: "${OPTIFLOW_API_PORT:=8000}"
export OPTIFLOW_WEB_PORT OPTIFLOW_API_PORT

printf 'Starting OptiFlow with Docker Compose.\n'
printf 'Web application: http://localhost:%s\n' "$OPTIFLOW_WEB_PORT"
printf 'API: http://localhost:%s\n' "$OPTIFLOW_API_PORT"
printf 'Press Ctrl+C to stop the stack.\n\n'

exec docker compose up --build
