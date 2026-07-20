@echo off
setlocal
cd /d "%~dp0"

where docker >nul 2>&1
if errorlevel 1 (
  echo Docker is required to run OptiFlow. 1>&2
  echo Install Docker Desktop for Windows and run this script again. 1>&2
  exit /b 1
)

docker compose version >nul 2>&1
if errorlevel 1 (
  echo The Docker Compose plugin is required. 1>&2
  echo Install or update Docker Desktop and run this script again. 1>&2
  exit /b 1
)

docker info >nul 2>&1
if errorlevel 1 (
  echo The Docker daemon is not available. 1>&2
  echo Start Docker Desktop and wait until it is ready. 1>&2
  exit /b 1
)

if not defined OPTIFLOW_WEB_PORT set "OPTIFLOW_WEB_PORT=8080"
if not defined OPTIFLOW_API_PORT set "OPTIFLOW_API_PORT=8000"

echo Starting OptiFlow with Docker Compose.
echo Web application: http://localhost:%OPTIFLOW_WEB_PORT%
echo API: http://localhost:%OPTIFLOW_API_PORT%
echo Press Ctrl+C to stop the stack.
echo.

docker compose up --build
exit /b %ERRORLEVEL%
