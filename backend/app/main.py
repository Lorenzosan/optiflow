from pathlib import Path
import os

from fastapi import FastAPI
from pydantic import BaseModel, Field


class ScenarioFiles(BaseModel):
    scenario: str = Field(description="Path to the scenario CSV, relative to the repository root.")
    prices: str = Field(description="Path to the prices CSV, relative to the repository root.")
    inflows: str = Field(description="Path to the inflows CSV, relative to the repository root.")


class ScenarioResponse(BaseModel):
    name: str
    description: str
    files: ScenarioFiles
    available: bool


class HealthResponse(BaseModel):
    status: str
    service: str


REPOSITORY_ROOT_ENV = "OPTIFLOW_REPO_ROOT"

SCENARIOS: tuple[tuple[str, str, ScenarioFiles], ...] = (
    (
        "synthetic_year",
        "Yearly pumped-storage and battery case with economically usable battery cycling.",
        ScenarioFiles(
            scenario="examples/yearly/scenario.csv",
            prices="examples/yearly/prices.csv",
            inflows="examples/yearly/inflows.csv",
        ),
    ),
    (
        "synthetic_year_no_battery",
        "Yearly case where the battery is physically unavailable.",
        ScenarioFiles(
            scenario="examples/yearly/scenario_no_battery.csv",
            prices="examples/yearly/prices.csv",
            inflows="examples/yearly/inflows.csv",
        ),
    ),
    (
        "synthetic_year_high_battery_degradation",
        "Yearly case where the battery is available but economically unattractive to cycle.",
        ScenarioFiles(
            scenario="examples/yearly/scenario_high_battery_degradation.csv",
            prices="examples/yearly/prices.csv",
            inflows="examples/yearly/inflows.csv",
        ),
    ),
)


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


app = FastAPI(
    title="OptiFlow API",
    description="Thin HTTP API for OptiFlow scenario discovery and future optimization runs.",
    version="0.1.0",
)


@app.get("/health", response_model=HealthResponse)
def health() -> HealthResponse:
    return HealthResponse(status="ok", service="optiflow-api")


@app.get("/scenarios", response_model=list[ScenarioResponse])
def list_scenarios() -> list[ScenarioResponse]:
    root = repository_root()
    return [
        ScenarioResponse(
            name=name,
            description=description,
            files=files,
            available=files_available(root, files),
        )
        for name, description, files in SCENARIOS
    ]
