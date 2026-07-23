"""@file
@brief FastAPI application and HTTP orchestration for OptiFlow.

The API persists scenario metadata and run summaries, safely exposes managed
CSV inputs and dispatch artifacts, and delegates optimization to the compiled
C++ solver. Numerical optimization logic intentionally remains outside the
Python service.
"""

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
from backend.app.models import OptimizationRun, RunSummary, Scenario
from backend.app.runner import RunSummaryData, resolve_dispatch_path, run_solver
from backend.app.scenario_inputs import ScenarioInputError, read_scenario_inputs
from backend.app.scenario_parameters import ScenarioParameterError, load_time_step_hours
from backend.app.scenario_uploads import (
    ScenarioUploadError,
    ScenarioValidatorError,
    managed_scenario_directory,
    remove_scenario_directory,
    store_validated_scenario,
)
from backend.app.seed import seed_scenarios
from backend.app.timestamps import as_utc, utc_now_naive


## @brief Module logger for orchestration and cleanup failures.
logger = logging.getLogger(__name__)


class ScenarioFiles(BaseModel):
    """Describe the three persisted CSV paths that define a scenario."""
    scenario: str = Field(description="Path to the scenario CSV, relative to the repository root.")
    prices: str = Field(description="Path to the prices CSV, relative to the repository root.")
    inflows: str = Field(description="Path to the inflows CSV, relative to the repository root.")


class ScenarioResponse(BaseModel):
    """Serialize scenario metadata, availability, editability, and time step."""
    id: int
    name: str
    description: str
    files: ScenarioFiles
    available: bool
    editable: bool
    time_step_hours: float | None


class ScenarioInputsResponse(BaseModel):
    """Return complete CSV inputs for browser-side scenario editing."""
    id: int
    name: str
    description: str
    editable: bool
    scenario_csv: str
    prices_csv: str
    inflows_csv: str


class HealthResponse(BaseModel):
    """Serialize the API health-check payload."""
    status: str
    service: str


class RunCreate(BaseModel):
    """Validate a request to execute one persisted scenario."""
    scenario_id: int = Field(description="Database id of the scenario to solve.")


## @brief Lifecycle states accepted by the run-history API filter.
RunStatus = Literal["pending", "running", "succeeded", "failed"]


class RunSummaryResponse(BaseModel):
    """Serialize scalar operational and economic metrics for a successful run."""
    net_operating_cashflow: float
    export_energy_mwh: float
    import_energy_mwh: float
    final_reservoir_volume: float
    solve_seconds: float
    simulation_seconds: float
    turbine_steps: int
    pump_steps: int
    spill_steps: int
    wait_steps: int


class RunResponse(BaseModel):
    """Serialize one optimization run with scenario metadata and optional summary."""
    id: int
    scenario_id: int
    scenario_name: str
    status: str
    started_at: datetime
    completed_at: datetime | None
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryResponse | None


class RunListResponse(BaseModel):
    """Serialize a paginated collection of optimization runs."""
    items: list[RunResponse]
    total: int
    limit: int
    offset: int


## @brief Environment variable overriding the backend repository root.
REPOSITORY_ROOT_ENV = "OPTIFLOW_REPO_ROOT"


def repository_root() -> Path:
    """Resolve the repository root used by backend filesystem operations.

    @return The resolved `OPTIFLOW_REPO_ROOT` value when configured, otherwise the
    root inferred from this module location.
    """
    configured_root = os.environ.get(REPOSITORY_ROOT_ENV)
    if configured_root:
        return Path(configured_root).resolve()
    return Path(__file__).resolve().parents[2]


def files_available(root: Path, files: ScenarioFiles) -> bool:
    """Check whether all three scenario input files currently exist.

    @param root Repository root used for stored relative paths.
    @param files Scenario file-path response model.
    @return `True` only when all three paths identify regular files.
    """
    return all(
        (root / relative_path).is_file()
        for relative_path in (files.scenario, files.prices, files.inflows)
    )


def scenario_response(root: Path, scenario: Scenario) -> ScenarioResponse:
    """Build an API scenario response from one database model.

    Availability and editability are calculated from current filesystem state. A
    malformed `time_step_hours` makes that optional field unavailable without
    preventing the scenario catalogue from loading.

    @param root Repository root used for filesystem checks.
    @param scenario Persisted scenario model.
    @return Serialized scenario response.
    """
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
        editable=managed_scenario_directory(
            root,
            scenario.scenario_path,
            scenario.prices_path,
            scenario.inflows_path,
        )
        is not None,
        time_step_hours=time_step_hours,
    )


@asynccontextmanager
async def lifespan(_: FastAPI) -> AsyncIterator[None]:
    """Seed bundled scenarios during FastAPI startup.

    @param _ FastAPI application instance supplied by the framework.
    @return Async context iterator controlling application lifespan.
    """
    with SessionLocal() as db:
        seed_scenarios(db)
    yield


## @brief ASGI application served by Uvicorn and proxied through NGINX.
app = FastAPI(
    title="OptiFlow API",
    description="HTTP orchestration for the OptiFlow pumped-storage optimizer.",
    version="0.1.0",
    lifespan=lifespan,
)


@app.get("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    """Return a lightweight service-health response.

    @return Static healthy status for the running API process.
    """
    return HealthResponse(status="ok", service="optiflow-api")


@app.get("/scenarios", response_model=list[ScenarioResponse])
def list_scenarios(db: Session = Depends(get_db)) -> list[ScenarioResponse]:
    """List persisted scenarios in stable database order.

    @param db Request-scoped SQLAlchemy session.
    @return Bundled and uploaded scenario responses with current availability.
    """
    root = repository_root()
    scenarios = db.query(Scenario).order_by(Scenario.id).all()
    return [scenario_response(root, scenario) for scenario in scenarios]


@app.get("/scenarios/{scenario_id}/inputs", response_model=ScenarioInputsResponse)
def get_scenario_inputs(
    scenario_id: int,
    db: Session = Depends(get_db),
) -> ScenarioInputsResponse:
    """Return complete CSV inputs for one scenario.

    Stored paths, file sizes, availability, and UTF-8 encoding are validated before
    contents are exposed.

    @param scenario_id Scenario database identifier.
    @param db Request-scoped SQLAlchemy session.
    @return Scenario metadata and the three CSV texts.
    @throws HTTPException With 404 when the scenario is absent or 409 when stored
    inputs cannot be read safely.
    """
    scenario = db.get(Scenario, scenario_id)
    if scenario is None:
        raise HTTPException(status_code=404, detail="Scenario not found")

    try:
        inputs = read_scenario_inputs(repository_root(), scenario)
    except ScenarioInputError as error:
        raise HTTPException(
            status_code=status.HTTP_409_CONFLICT,
            detail=str(error),
        ) from error

    return ScenarioInputsResponse(
        id=scenario.id,
        name=scenario.name,
        description=scenario.description,
        editable=inputs.editable,
        scenario_csv=inputs.scenario_csv,
        prices_csv=inputs.prices_csv,
        inflows_csv=inputs.inflows_csv,
    )


@app.post(
    "/scenarios",
    response_model=ScenarioResponse,
    status_code=status.HTTP_201_CREATED,
)
async def create_scenario(
    scenario_file: Annotated[UploadFile, File(alias="scenario")],
    prices_file: Annotated[UploadFile, File(alias="prices")],
    inflows_file: Annotated[UploadFile, File(alias="inflows")],
    description: Annotated[str, Form(max_length=2000)] = "",
    overwrite: Annotated[bool, Form()] = False,
    db: Session = Depends(get_db),
) -> ScenarioResponse:
    """Validate and persist a custom uploaded scenario.

    Files are staged and optimizer-validated before any database mutation. Name
    collisions require explicit overwrite, and bundled scenarios cannot be
    replaced. A successful custom overwrite deletes prior runs and their managed
    dispatch artifacts after the replacement is committed.

    @param scenario_file Scalar scenario CSV upload.
    @param prices_file Electricity-price CSV upload.
    @param inflows_file Natural-inflow CSV upload.
    @param description Optional user-facing scenario description.
    @param overwrite Whether an existing custom scenario with the same name may be
    replaced.
    @param db Request-scoped SQLAlchemy session.
    @return Created or overwritten scenario response.
    @throws HTTPException For invalid uploads, name conflicts, immutable bundled
    scenarios, or unavailable validation infrastructure.
    """
    normalized_description = description.strip()

    root = repository_root()
    try:
        # Validate and publish files before mutating scenario database records.
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
        if not overwrite:
            remove_scenario_directory(stored.directory)
            raise HTTPException(status_code=409, detail="Scenario name already exists")

        previous_directory = managed_scenario_directory(
            root,
            existing.scenario_path,
            existing.prices_path,
            existing.inflows_path,
        )
        if previous_directory is None:
            remove_scenario_directory(stored.directory)
            raise HTTPException(
                status_code=409,
                detail="Bundled scenarios cannot be overwritten",
            )

        # Capture managed artifacts before deleting the previous run graph.
        dispatch_artifacts: list[Path] = []
        for run in list(existing.runs):
            if run.output_dispatch_path:
                artifact = resolve_dispatch_path(root, run.output_dispatch_path)
                if artifact is not None:
                    dispatch_artifacts.append(artifact)
            db.delete(run)

        existing.description = normalized_description
        existing.scenario_path = stored.scenario_path
        existing.prices_path = stored.prices_path
        existing.inflows_path = stored.inflows_path
        existing.created_at = utc_now_naive()
        try:
            db.commit()
            db.refresh(existing)
        except Exception:
            db.rollback()
            remove_scenario_directory(stored.directory)
            logger.exception("Failed to overwrite uploaded scenario %s", stored.name)
            raise

        # Database replacement is durable before obsolete filesystem data is removed.
        remove_scenario_directory(previous_directory)
        for artifact in dispatch_artifacts:
            try:
                artifact.unlink(missing_ok=True)
            except OSError:
                logger.warning("Could not remove obsolete dispatch artifact %s", artifact)
        return scenario_response(root, existing)

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
    """Convert an optional persisted run summary into its API model.

    @param summary Persisted summary or `None`.
    @return Serialized summary or `None`.
    """
    if summary is None:
        return None
    return RunSummaryResponse(
        net_operating_cashflow=summary.net_operating_cashflow,
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
    """Attach validated solver metrics to a run as a new ORM summary.

    @param run Optimization run receiving the one-to-one summary.
    @param summary Validated solver summary data.
    """
    run.summary = RunSummary(
        net_operating_cashflow=summary.net_operating_cashflow,
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


def run_response(run: OptimizationRun) -> RunResponse:
    """Convert an optimization run ORM graph into its API representation.

    Database timestamps are normalized to aware UTC values.

    @param run Run with its `scenario` and optional `summary` relationships loaded.
    @return Serialized run response.
    """
    return RunResponse(
        id=run.id,
        scenario_id=run.scenario_id,
        scenario_name=run.scenario.name,
        status=run.status,
        started_at=as_utc(run.started_at),
        completed_at=(
            as_utc(run.completed_at) if run.completed_at is not None else None
        ),
        output_dispatch_path=run.output_dispatch_path,
        error_message=run.error_message,
        summary=summary_response(run.summary),
    )


@app.post("/runs", response_model=RunResponse, status_code=status.HTTP_201_CREATED)
def create_run(request: RunCreate, db: Session = Depends(get_db)) -> RunResponse:
    """Execute one scenario synchronously and persist its result.

    A running row is committed before invoking the C++ process, ensuring failures
    remain visible in run history. Expected solver failures are returned as normal
    failed run responses; unexpected orchestration failures produce HTTP 500 while
    also persisting a failed run state.

    @param request Scenario identifier to execute.
    @param db Request-scoped SQLAlchemy session.
    @return Completed succeeded or failed run response.
    @throws HTTPException With 404 for an unknown scenario or 500 for unexpected
    execution failures.
    """
    scenario = db.get(Scenario, request.scenario_id)
    if scenario is None:
        raise HTTPException(status_code=404, detail="Scenario not found")

    # Persist the running state before the synchronous subprocess blocks the request.
    run = OptimizationRun(
        scenario_id=scenario.id,
        status="running",
        started_at=utc_now_naive(),
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
        run.completed_at = utc_now_naive()
        run.output_dispatch_path = None
        run.error_message = "Unexpected solver execution error. See service logs for details."
        db.commit()
        raise HTTPException(
            status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
            detail={"message": "Optimization run failed unexpectedly", "run_id": run.id},
        )

    run.status = result.status
    run.completed_at = utc_now_naive()
    run.output_dispatch_path = result.output_dispatch_path
    run.error_message = result.error_message
    if result.summary is not None:
        attach_summary(run, result.summary)
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
    """Return filtered, newest-first optimization run history.

    @param limit Maximum number of rows to return.
    @param offset Number of matching rows to skip.
    @param scenario_id Optional scenario filter.
    @param run_status Optional lifecycle-status filter exposed as the `status`
    query parameter.
    @param db Request-scoped SQLAlchemy session.
    @return Paginated run list with total matching count.
    """
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
    """Return one optimization run by identifier.

    @param run_id Run database identifier.
    @param db Request-scoped SQLAlchemy session.
    @return Serialized run response.
    @throws HTTPException With 404 when the run does not exist.
    """
    run = db.get(OptimizationRun, run_id)
    if run is None:
        raise HTTPException(status_code=404, detail="Run not found")
    return run_response(run)


@app.get("/runs/{run_id}/dispatch.csv", response_class=FileResponse)
def download_dispatch(run_id: int, db: Session = Depends(get_db)) -> FileResponse:
    """Return the managed dispatch CSV for a successful run.

    The persisted artifact path is constrained to the configured run-output root
    before the file is served.

    @param run_id Run database identifier.
    @param db Request-scoped SQLAlchemy session.
    @return CSV file response.
    @throws HTTPException When the run is absent, incomplete, malformed, or its
    artifact is unavailable.
    """
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
