from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
import logging
import os

from fastapi import Depends, FastAPI, HTTPException, status
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session

from backend.app.database import SessionLocal, get_db
from backend.app.models import OptimizationRun, RunSummary, Scenario
from backend.app.runner import RunSummaryData, resolve_dispatch_path, run_solver
from backend.app.seed import seed_scenarios


logger = logging.getLogger(__name__)


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


class RunSummaryResponse(BaseModel):
    cumulative_profit: float
    export_energy_mwh: float
    import_energy_mwh: float
    final_reservoir_volume: float
    final_battery_soc: float
    solve_seconds: float
    simulation_seconds: float
    turbine_steps: int
    pump_steps: int
    spill_steps: int
    battery_charge_steps: int
    battery_discharge_steps: int
    wait_steps: int


class RunResponse(BaseModel):
    id: int
    scenario_id: int
    scenario_name: str
    status: str
    started_at: datetime
    completed_at: datetime | None
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryResponse | None


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


def summary_response(summary: RunSummary | None) -> RunSummaryResponse | None:
    if summary is None:
        return None
    return RunSummaryResponse(
        cumulative_profit=summary.cumulative_profit,
        export_energy_mwh=summary.export_energy_mwh,
        import_energy_mwh=summary.import_energy_mwh,
        final_reservoir_volume=summary.final_reservoir_volume,
        final_battery_soc=summary.final_battery_soc,
        solve_seconds=summary.solve_seconds,
        simulation_seconds=summary.simulation_seconds,
        turbine_steps=summary.turbine_steps,
        pump_steps=summary.pump_steps,
        spill_steps=summary.spill_steps,
        battery_charge_steps=summary.battery_charge_steps,
        battery_discharge_steps=summary.battery_discharge_steps,
        wait_steps=summary.wait_steps,
    )


def attach_summary(run: OptimizationRun, summary: RunSummaryData) -> None:
    run.summary = RunSummary(
        cumulative_profit=summary.cumulative_profit,
        export_energy_mwh=summary.export_energy_mwh,
        import_energy_mwh=summary.import_energy_mwh,
        final_reservoir_volume=summary.final_reservoir_volume,
        final_battery_soc=summary.final_battery_soc,
        solve_seconds=summary.solve_seconds,
        simulation_seconds=summary.simulation_seconds,
        turbine_steps=summary.turbine_steps,
        pump_steps=summary.pump_steps,
        spill_steps=summary.spill_steps,
        battery_charge_steps=summary.battery_charge_steps,
        battery_discharge_steps=summary.battery_discharge_steps,
        wait_steps=summary.wait_steps,
    )


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
        summary=summary_response(run.summary),
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

    try:
        result = run_solver(repository_root(), scenario, run.id)
        if result.status == "succeeded" and result.summary is None:
            raise RuntimeError("successful solver result did not include a run summary")
        if result.status != "succeeded" and result.summary is not None:
            raise RuntimeError("failed solver result included a run summary")
    except Exception:
        logger.exception("Unexpected error while executing optimization run %s", run.id)
        run.status = "failed"
        run.completed_at = datetime.utcnow()
        run.output_dispatch_path = None
        run.error_message = "Unexpected solver execution error. See service logs for details."
        db.commit()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={
                "message": "Optimization run failed unexpectedly",
                "run_id": run.id,
            },
        )

    run.status = result.status
    run.completed_at = datetime.utcnow()
    run.output_dispatch_path = result.output_dispatch_path
    run.error_message = result.error_message
    if result.summary is not None:
        attach_summary(run, result.summary)
    db.commit()
    db.refresh(run)

    return run_response(run)


@app.get("/runs/{run_id}", response_model=RunResponse)
def get_run(run_id: int, db: Session = Depends(get_db)) -> RunResponse:
    run = db.get(OptimizationRun, run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run_response(run)


@app.get("/runs/{run_id}/dispatch.csv", response_class=FileResponse)
def download_dispatch(run_id: int, db: Session = Depends(get_db)) -> FileResponse:
    run = db.get(OptimizationRun, run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    if run.status != "succeeded":
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail="Dispatch is available only for succeeded runs",
        )
    if run.output_dispatch_path is None:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Succeeded run has no dispatch artifact path",
        )

    artifact_path = resolve_dispatch_path(repository_root(), run.output_dispatch_path)
    if artifact_path is None:
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Dispatch artifact path is invalid",
        )
    if not artifact_path.is_file():
        raise HTTPException(status_code=404, detail="Dispatch artifact not found")

    return FileResponse(
        path=artifact_path,
        media_type="text/csv",
        filename=artifact_path.name,
    )
