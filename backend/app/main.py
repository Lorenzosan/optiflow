from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
from typing import Annotated, Literal
import logging
import os

from fastapi import Depends, FastAPI, File, Form, HTTPException, Query, UploadFile, status
from fastapi.responses import FileResponse
from pydantic import BaseModel, Field
from sqlalchemy.exc import IntegrityError
from sqlalchemy.orm import Session, joinedload

from backend.app.database import SessionLocal, get_db
from backend.app.models import OptimizationRun, RunProvenance, RunSummary, Scenario
from backend.app.provenance import RunProvenanceData
from backend.app.runner import RunSummaryData, resolve_dispatch_path, run_solver
from backend.app.scenario_parameters import ScenarioParameterError, load_time_step_hours
from backend.app.scenario_uploads import (
    ScenarioUploadError,
    ScenarioValidatorError,
    remove_scenario_directory,
    store_validated_scenario,
)
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
    time_step_hours: float | None


class HealthResponse(BaseModel):
    status: str
    service: str


class RunCreate(BaseModel):
    scenario_id: int = Field(description="Database id of the scenario to solve.")


RunStatus = Literal["pending", "running", "succeeded", "failed"]


class RunSummaryResponse(BaseModel):
    cumulative_profit: float
    export_energy_mwh: float
    import_energy_mwh: float
    final_reservoir_volume: float
    solve_seconds: float
    simulation_seconds: float
    turbine_steps: int
    pump_steps: int
    spill_steps: int
    wait_steps: int


class RunProvenanceResponse(BaseModel):
    result_schema_version: int
    scenario_sha256: str
    prices_sha256: str
    inflows_sha256: str
    solver_sha256: str
    dispatch_sha256: str | None
    horizon_steps: int
    reservoir_volume_grid_points: int
    turbine_flow_steps: int
    pump_flow_steps: int
    spill_flow_steps: int


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
    provenance: RunProvenanceResponse | None


class RunListResponse(BaseModel):
    items: list[RunResponse]
    total: int
    limit: int
    offset: int


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


def scenario_response(root: Path, scenario: Scenario) -> ScenarioResponse:
    files = ScenarioFiles(
        scenario=scenario.scenario_path,
        prices=scenario.prices_path,
        inflows=scenario.inflows_path,
    )
    available = files_available(root, files)
    time_step_hours = None
    if available:
        try:
            time_step_hours = load_time_step_hours(root / files.scenario)
        except ScenarioParameterError:
            logger.warning(
                "Could not read time_step_hours for scenario %s",
                scenario.name,
                exc_info=True,
            )
    return ScenarioResponse(
        id=scenario.id,
        name=scenario.name,
        description=scenario.description,
        files=files,
        available=available,
        time_step_hours=time_step_hours,
    )


@asynccontextmanager
async def lifespan(_: FastAPI) -> AsyncIterator[None]:
    with SessionLocal() as db:
        seed_scenarios(db)
    yield


app = FastAPI(
    title="OptiFlow API",
    description="HTTP orchestration for the OptiFlow pumped-storage optimizer.",
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
    return [scenario_response(root, scenario) for scenario in scenarios]


@app.post(
    "/scenarios",
    response_model=ScenarioResponse,
    status_code=status.HTTP_201_CREATED,
)
async def create_scenario(
    description: Annotated[str, Form(max_length=2000)],
    scenario_file: Annotated[UploadFile, File(alias="scenario")],
    prices_file: Annotated[UploadFile, File(alias="prices")],
    inflows_file: Annotated[UploadFile, File(alias="inflows")],
    db: Session = Depends(get_db),
) -> ScenarioResponse:
    normalized_description = description.strip()
    if not normalized_description:
        raise HTTPException(status_code=422, detail="Description must not be empty")

    root = repository_root()
    try:
        stored = await store_validated_scenario(
            root,
            scenario_file,
            prices_file,
            inflows_file,
        )
    except ScenarioUploadError as error:
        raise HTTPException(
            status_code=422,
            detail={"message": "Scenario validation failed", "error": str(error)},
        ) from error
    except ScenarioValidatorError as error:
        logger.exception("Scenario validation infrastructure failed")
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail="Scenario validation service is unavailable",
        ) from error

    existing = db.query(Scenario).filter(Scenario.name == stored.name).one_or_none()
    if existing is not None:
        remove_scenario_directory(stored.directory)
        raise HTTPException(status_code=409, detail="Scenario name already exists")

    scenario = Scenario(
        name=stored.name,
        description=normalized_description,
        scenario_path=stored.scenario_path,
        prices_path=stored.prices_path,
        inflows_path=stored.inflows_path,
    )
    db.add(scenario)
    try:
        db.commit()
        db.refresh(scenario)
    except IntegrityError as error:
        db.rollback()
        remove_scenario_directory(stored.directory)
        raise HTTPException(status_code=409, detail="Scenario name already exists") from error
    except Exception:
        db.rollback()
        remove_scenario_directory(stored.directory)
        logger.exception("Failed to persist uploaded scenario %s", stored.name)
        raise

    return scenario_response(root, scenario)


def summary_response(summary: RunSummary | None) -> RunSummaryResponse | None:
    if summary is None:
        return None
    return RunSummaryResponse(
        cumulative_profit=summary.cumulative_profit,
        export_energy_mwh=summary.export_energy_mwh,
        import_energy_mwh=summary.import_energy_mwh,
        final_reservoir_volume=summary.final_reservoir_volume,
        solve_seconds=summary.solve_seconds,
        simulation_seconds=summary.simulation_seconds,
        turbine_steps=summary.turbine_steps,
        pump_steps=summary.pump_steps,
        spill_steps=summary.spill_steps,
        wait_steps=summary.wait_steps,
    )


def attach_summary(run: OptimizationRun, summary: RunSummaryData) -> None:
    run.summary = RunSummary(
        cumulative_profit=summary.cumulative_profit,
        export_energy_mwh=summary.export_energy_mwh,
        import_energy_mwh=summary.import_energy_mwh,
        final_reservoir_volume=summary.final_reservoir_volume,
        solve_seconds=summary.solve_seconds,
        simulation_seconds=summary.simulation_seconds,
        turbine_steps=summary.turbine_steps,
        pump_steps=summary.pump_steps,
        spill_steps=summary.spill_steps,
        wait_steps=summary.wait_steps,
    )


def provenance_response(
    provenance: RunProvenance | None,
) -> RunProvenanceResponse | None:
    if provenance is None:
        return None
    return RunProvenanceResponse(
        result_schema_version=provenance.result_schema_version,
        scenario_sha256=provenance.scenario_sha256,
        prices_sha256=provenance.prices_sha256,
        inflows_sha256=provenance.inflows_sha256,
        solver_sha256=provenance.solver_sha256,
        dispatch_sha256=provenance.dispatch_sha256,
        horizon_steps=provenance.horizon_steps,
        reservoir_volume_grid_points=provenance.reservoir_volume_grid_points,
        turbine_flow_steps=provenance.turbine_flow_steps,
        pump_flow_steps=provenance.pump_flow_steps,
        spill_flow_steps=provenance.spill_flow_steps,
    )


def attach_provenance(run: OptimizationRun, provenance: RunProvenanceData) -> None:
    run.provenance = RunProvenance(
        result_schema_version=provenance.result_schema_version,
        scenario_sha256=provenance.scenario_sha256,
        prices_sha256=provenance.prices_sha256,
        inflows_sha256=provenance.inflows_sha256,
        solver_sha256=provenance.solver_sha256,
        dispatch_sha256=provenance.dispatch_sha256,
        horizon_steps=provenance.horizon_steps,
        reservoir_volume_grid_points=provenance.reservoir_volume_grid_points,
        turbine_flow_steps=provenance.turbine_flow_steps,
        pump_flow_steps=provenance.pump_flow_steps,
        spill_flow_steps=provenance.spill_flow_steps,
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
        provenance=provenance_response(run.provenance),
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
        if result.status == "succeeded" and result.provenance is None:
            raise RuntimeError("successful solver result did not include run provenance")
        if (
            result.status == "succeeded"
            and result.provenance is not None
            and result.provenance.dispatch_sha256 is None
        ):
            raise RuntimeError("successful solver provenance did not include a dispatch hash")
        if result.status != "succeeded" and result.summary is not None:
            raise RuntimeError("failed solver result included a run summary")
        if (
            result.status != "succeeded"
            and result.provenance is not None
            and result.provenance.dispatch_sha256 is not None
        ):
            raise RuntimeError("failed solver provenance included a dispatch hash")
    except Exception:
        logger.exception("Unexpected error while executing optimization run %s", run.id)
        run.status = "failed"
        run.completed_at = datetime.utcnow()
        run.output_dispatch_path = None
        run.error_message = "Unexpected solver execution error. See service logs for details."
        db.commit()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={"message": "Optimization run failed unexpectedly", "run_id": run.id},
        )

    run.status = result.status
    run.completed_at = datetime.utcnow()
    run.output_dispatch_path = result.output_dispatch_path
    run.error_message = result.error_message
    if result.summary is not None:
        attach_summary(run, result.summary)
    if result.provenance is not None:
        attach_provenance(run, result.provenance)
    db.commit()
    db.refresh(run)
    return run_response(run)


@app.get("/runs", response_model=RunListResponse)
def list_runs(
    limit: int = Query(default=20, ge=1, le=100),
    offset: int = Query(default=0, ge=0),
    scenario_id: int | None = Query(default=None, gt=0),
    run_status: RunStatus | None = Query(default=None, alias="status"),
    db: Session = Depends(get_db),
) -> RunListResponse:
    query = db.query(OptimizationRun)
    if scenario_id is not None:
        query = query.filter(OptimizationRun.scenario_id == scenario_id)
    if run_status is not None:
        query = query.filter(OptimizationRun.status == run_status)

    total = query.count()
    runs = (
        query.options(
            joinedload(OptimizationRun.scenario),
            joinedload(OptimizationRun.summary),
            joinedload(OptimizationRun.provenance),
        )
        .order_by(OptimizationRun.started_at.desc(), OptimizationRun.id.desc())
        .offset(offset)
        .limit(limit)
        .all()
    )
    return RunListResponse(
        items=[run_response(run) for run in runs],
        total=total,
        limit=limit,
        offset=offset,
    )


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
