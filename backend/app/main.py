from collections.abc import AsyncIterator
from contextlib import asynccontextmanager
from pathlib import Path
import os

from fastapi import Depends, FastAPI
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session

from backend.app.database import SessionLocal, create_schema, get_db
from backend.app.models import Scenario
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
