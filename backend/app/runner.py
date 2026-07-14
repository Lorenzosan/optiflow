from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Literal
import json
import os
import subprocess

from pydantic import BaseModel, ConfigDict, ValidationError

from backend.app.models import Scenario
from backend.app.provenance import (
    ProvenanceError,
    RunProvenanceData,
    collect_run_provenance,
    sha256_file,
)


RUN_OUTPUT_DIR_ENV = "OPTIFLOW_RUN_OUTPUT_DIR"
SOLVE_BIN_ENV = "OPTIFLOW_SOLVE_BIN"
DEFAULT_RUN_OUTPUT_DIR = "build/api-runs"
DEFAULT_SOLVE_BIN = "build/apps/solve_cli/optiflow_solve"
SOLVER_TIMEOUT_SECONDS = 600
MAX_ERROR_LENGTH = 4000


class RunSummaryData(BaseModel):
    model_config = ConfigDict(extra="forbid")

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


@dataclass(frozen=True)
class SolverResult:
    status: Literal["succeeded", "failed"]
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryData | None
    provenance: RunProvenanceData | None


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

    solver_path = solve_executable(root)
    try:
        provenance = collect_run_provenance(root, scenario, solver_path)
    except ProvenanceError as error:
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(f"Run provenance is unavailable: {error}"),
            summary=None,
            provenance=None,
        )

    command = [
        str(solver_path),
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
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message="Solver timed out.",
            summary=None,
            provenance=provenance,
        )
    except OSError as error:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(str(error)),
            summary=None,
            provenance=provenance,
        )

    if completed.returncode != 0:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(completed.stderr or completed.stdout),
            summary=None,
            provenance=provenance,
        )
    if not dispatch_path.is_file():
        summary_path.unlink(missing_ok=True)
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message="Solver did not create a dispatch artifact.",
            summary=None,
            provenance=provenance,
        )

    try:
        summary = read_summary(summary_path)
        dispatch_sha256 = sha256_file(dispatch_path)
    except (ValueError, ProvenanceError) as error:
        dispatch_path.unlink(missing_ok=True)
        summary_path.unlink(missing_ok=True)
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(str(error)),
            summary=None,
            provenance=provenance,
        )
    summary_path.unlink(missing_ok=True)
    return SolverResult(
        status="succeeded",
        output_dispatch_path=display_path(root, dispatch_path),
        error_message=None,
        summary=summary,
        provenance=provenance.with_dispatch_sha256(dispatch_sha256),
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
