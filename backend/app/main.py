from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
import os

from fastapi import Depends, FastAPI, HTTPException, status
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session

from backend.app.database import SessionLocal, create_schema, get_db
from backend.app.models import OptimizationRun, Scenario
from backend.app.runner import run_solver
from backend.app.seed import seed_scenarios


class ScenarioFiles(BaseModel):
    scenario: str = Field(description="Path to the scenario CSV, relative to the repository root.")
    prices: str = Field(description="Path to the prices CSV, relative to the repository root.")
    inflows: str = Field(description="Path to the inflows CSV, relative to the repository root.")


class ScenarioResponse(BaseModel):
    id: int
    name: str
    description: str
    files: ScenarioFiles
    available: bool


class HealthResponse(BaseModel):
    status: str
    service: str


class RunCreate(BaseModel):
    scenario_id: int = Field(description="Database id of the scenario to solve.")


class RunResponse(BaseModel):
    id: int
    scenario_id: int
    scenario_name: str
    status: str
    started_at: datetime
    completed_at: datetime | None
    output_dispatch_path: str | None
    error_message: str | None


REPOSITORY_ROOT_ENV = "OPTIFLOW_REPO_ROOT"


def repository_root() -> Path:
    configured_root = os.environ.get(REPOSITORY_ROOT_ENV)
    if configured_root:
        return Path(configured_root).resolve()
    return Path(__file__).resolve().parents[2]


def files_available(root: Path, files: ScenarioFiles) -> bool:
    return all(
        (root / relative_path).is_file()
        for relative_path in (files.scenario, files.prices, files.inflows)
    )


@asynccontextmanager
async def lifespan(_: FastAPI) -> AsyncIterator[None]:
    create_schema()
    with SessionLocal() as db:
        seed_scenarios(db)
    yield


app = FastAPI(
    title="OptiFlow API",
    description="Thin HTTP API for OptiFlow scenario discovery and future optimization runs.",
    version="0.1.0",
    lifespan=lifespan,
)


@app.get("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    return HealthResponse(status="ok", service="optiflow-api")


@app.get("/scenarios", response_model=list[ScenarioResponse])
def list_scenarios(db: Session = Depends(get_db)) -> list[ScenarioResponse]:
    root = repository_root()
    scenarios = db.query(Scenario).order_by(Scenario.id).all()
    responses: list[ScenarioResponse] = []
    for scenario in scenarios:
        files = ScenarioFiles(
            scenario=scenario.scenario_path,
            prices=scenario.prices_path,
            inflows=scenario.inflows_path,
        )
        responses.append(
            ScenarioResponse(
                id=scenario.id,
                name=scenario.name,
                description=scenario.description,
                files=files,
                available=files_available(root, files),
            )
        )
    return responses


def run_response(run: OptimizationRun) -> RunResponse:
    return RunResponse(
        id=run.id,
        scenario_id=run.scenario_id,
        scenario_name=run.scenario.name,
        status=run.status,
        started_at=run.started_at,
        completed_at=run.completed_at,
        output_dispatch_path=run.output_dispatch_path,
        error_message=run.error_message,
    )


@app.post("/runs", response_model=RunResponse, status_code=status.HTTP_201_CREATED)
def create_run(request: RunCreate, db: Session = Depends(get_db)) -> RunResponse:
    scenario = db.get(Scenario, request.scenario_id)
    if scenario is None:
        raise HTTPException(status_code=404, detail="Scenario not found")

    run = OptimizationRun(
        scenario_id=scenario.id,
        status="running",
        started_at=datetime.utcnow(),
    )
    db.add(run)
    db.commit()
    db.refresh(run)

    result = run_solver(repository_root(), scenario, run.id)
    run.status = result.status
    run.completed_at = datetime.utcnow()
    run.output_dispatch_path = result.output_dispatch_path
    run.error_message = result.error_message
    db.commit()
    db.refresh(run)

    return run_response(run)


@app.get("/runs/{run_id}", response_model=RunResponse)
def get_run(run_id: int, db: Session = Depends(get_db)) -> RunResponse:
    run = db.get(OptimizationRun, run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run_response(run)
