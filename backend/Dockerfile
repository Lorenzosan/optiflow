FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1
ENV PYTHONUNBUFFERED=1
ENV OPTIFLOW_REPO_ROOT=/app
ENV OPTIFLOW_SOLVE_BIN=/app/build/apps/solve_cli/optiflow_solve
ENV OPTIFLOW_RUN_OUTPUT_DIR=/app/build/api-runs
ENV OPTIFLOW_SCENARIO_STORAGE_DIR=/app/data/scenarios

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake \
    && rm -rf /var/lib/apt/lists/*

COPY alembic.ini /app/alembic.ini
COPY backend/requirements.txt /app/backend/requirements.txt
RUN pip install --no-cache-dir -r /app/backend/requirements.txt

COPY CMakeLists.txt /app/CMakeLists.txt
COPY apps /app/apps
COPY libs /app/libs
COPY backend /app/backend
COPY examples /app/examples

RUN cmake -S /app -B /app/build -DCMAKE_BUILD_TYPE=Release -DOPTIFLOW_BUILD_TESTS=OFF \
    && cmake --build /app/build -j

EXPOSE 8000

CMD ["sh", "-c", "python -m alembic upgrade head && exec uvicorn backend.app.main:app --host 0.0.0.0 --port 8000"]
