from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Literal
import json
import os
import subprocess

from pydantic import BaseModel, ConfigDict, ValidationError

from backend.app.models import Scenario


RUN_OUTPUT_DIR_ENV = "OPTIFLOW_RUN_OUTPUT_DIR"
SOLVE_BIN_ENV = "OPTIFLOW_SOLVE_BIN"
DEFAULT_RUN_OUTPUT_DIR = "build/api-runs"
DEFAULT_SOLVE_BIN = "build/apps/solve_cli/optiflow_solve"
SOLVER_TIMEOUT_SECONDS = 600
MAX_ERROR_LENGTH = 4000


class RunSummaryData(BaseModel):
    model_config = ConfigDict(extra="forbid")

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


@dataclass(frozen=True)
class SolverResult:
    status: Literal["succeeded", "failed"]
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryData | None


def display_path(root: Path, path: Path) -> str:
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def solve_executable(root: Path) -> Path:
    configured = Path(os.environ.get(SOLVE_BIN_ENV, DEFAULT_SOLVE_BIN)).expanduser()
    return configured if configured.is_absolute() else root / configured


def run_output_dir(root: Path) -> Path:
    configured = Path(os.environ.get(RUN_OUTPUT_DIR_ENV, DEFAULT_RUN_OUTPUT_DIR)).expanduser()
    return configured if configured.is_absolute() else root / configured


def truncate_error(message: str) -> str:
    normalized = message.strip() or "Solver failed without an error message."
    return normalized[:MAX_ERROR_LENGTH]


def read_summary(path: Path) -> RunSummaryData:
    try:
        payload = json.loads(path.read_text())
        return RunSummaryData.model_validate(payload)
    except (OSError, json.JSONDecodeError, ValidationError) as error:
        raise ValueError(f"Solver summary JSON is invalid: {error}") from error


def run_solver(root: Path, scenario: Scenario, run_id: int) -> SolverResult:
    output_dir = run_output_dir(root)
    output_dir.mkdir(parents=True, exist_ok=True)
    dispatch_path = output_dir / f"run_{run_id:06d}_dispatch.csv"
    summary_path = output_dir / f"run_{run_id:06d}_summary.json"
    dispatch_path.unlink(missing_ok=True)
    summary_path.unlink(missing_ok=True)

    command = [
        str(solve_executable(root)),
        "--scenario", str(root / scenario.scenario_path),
        "--prices", str(root / scenario.prices_path),
        "--inflows", str(root / scenario.inflows_path),
        "--output", str(dispatch_path),
        "--summary-output", str(summary_path),
    ]
    try:
        completed = subprocess.run(
            command,
            cwd=root,
            text=True,
            capture_output=True,
            timeout=SOLVER_TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult("failed", None, "Solver timed out.", None)
    except OSError as error:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult("failed", None, truncate_error(str(error)), None)

    if completed.returncode != 0:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult(
            "failed",
            None,
            truncate_error(completed.stderr or completed.stdout),
            None,
        )
    if not dispatch_path.is_file():
        summary_path.unlink(missing_ok=True)
        return SolverResult("failed", None, "Solver did not create a dispatch artifact.", None)

    try:
        summary = read_summary(summary_path)
    except ValueError as error:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult("failed", None, truncate_error(str(error)), None)
    summary_path.unlink(missing_ok=True)
    return SolverResult(
        "succeeded",
        display_path(root, dispatch_path),
        None,
        summary,
    )


def resolve_dispatch_path(root: Path, stored_path: str) -> Path | None:
    output_root = run_output_dir(root).resolve()
    candidate = Path(stored_path)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        resolved = candidate.resolve()
        resolved.relative_to(output_root)
    except (OSError, ValueError):
        return None
    return resolved
