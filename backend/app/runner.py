"""@file
@brief Execute the compiled OptiFlow solver and validate its output artifacts.

The FastAPI service treats the C++ command-line program as the numerical
optimization boundary. This module prepares command arguments, enforces a
timeout, validates summary JSON, and constrains dispatch paths to managed
storage.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Literal
import json
import os
import subprocess

from pydantic import BaseModel, ConfigDict, ValidationError

from backend.app.models import Scenario


## @brief Environment variable overriding the managed run-artifact directory.
RUN_OUTPUT_DIR_ENV = "OPTIFLOW_RUN_OUTPUT_DIR"

## @brief Environment variable overriding the compiled solver executable.
SOLVE_BIN_ENV = "OPTIFLOW_SOLVE_BIN"

## @brief Repository-relative default for generated run artifacts.
DEFAULT_RUN_OUTPUT_DIR = "build/api-runs"

## @brief Repository-relative default for the C++ solver executable.
DEFAULT_SOLVE_BIN = "build/apps/solve_cli/optiflow_solve"

## @brief Maximum wall-clock duration allowed for one solver process.
SOLVER_TIMEOUT_SECONDS = 600

## @brief Maximum persisted solver error-message length.
MAX_ERROR_LENGTH = 4000


class RunSummaryData(BaseModel):
    """Validate the scalar JSON summary emitted by the C++ solver.

    Unknown fields are rejected so backend persistence cannot silently accept a
    changed solver-output contract.
    """
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
    """Represent the normalized outcome of one solver subprocess.

    Successful results contain a managed dispatch path and validated summary.
    Failed results contain a bounded error message and no output artifacts.
    """
    status: Literal["succeeded", "failed"]
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryData | None


def display_path(root: Path, path: Path) -> str:
    """Format a filesystem path for database persistence and diagnostics.

    @param root Repository root used as the preferred relative-path base.
    @param path Path to format.
    @return A POSIX path relative to `root` when possible, otherwise an absolute
    resolved path.
    """
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(path.resolve())


def solve_executable(root: Path) -> Path:
    """Resolve the configured C++ solver executable.

    @param root Repository root used for relative configuration values.
    @return Absolute or repository-relative executable path selected through
    `OPTIFLOW_SOLVE_BIN`.
    """
    configured = Path(os.environ.get(SOLVE_BIN_ENV, DEFAULT_SOLVE_BIN)).expanduser()
    return configured if configured.is_absolute() else root / configured


def run_output_dir(root: Path) -> Path:
    """Resolve the managed directory for generated run artifacts.

    @param root Repository root used for relative configuration values.
    @return Absolute output directory selected through `OPTIFLOW_RUN_OUTPUT_DIR`.
    """
    configured = Path(os.environ.get(RUN_OUTPUT_DIR_ENV, DEFAULT_RUN_OUTPUT_DIR)).expanduser()
    return configured if configured.is_absolute() else root / configured


def truncate_error(message: str) -> str:
    """Normalize and bound a solver-facing error message.

    @param message Raw standard-error, standard-output, or exception text.
    @return A nonempty message truncated to `MAX_ERROR_LENGTH` characters.
    """
    normalized = message.strip() or "Solver failed without an error message."
    return normalized[:MAX_ERROR_LENGTH]


def read_summary(path: Path) -> RunSummaryData:
    """Read and validate one solver summary JSON file.

    @param path Summary artifact path.
    @return A validated `RunSummaryData` instance.
    @throws ValueError When the file cannot be read, parsed, or validated.
    """
    try:
        payload = json.loads(path.read_text())
        return RunSummaryData.model_validate(payload)
    except (OSError, json.JSONDecodeError, ValidationError) as error:
        raise ValueError(f"Solver summary JSON is invalid: {error}") from error


def run_solver(root: Path, scenario: Scenario, run_id: int) -> SolverResult:
    """Execute the C++ optimizer for one persisted scenario.

    Existing artifacts for the run identifier are removed before execution. Failed
    or incomplete executions are normalized into `SolverResult` values and leave no
    stale dispatch or summary file behind.

    @param root Repository root containing scenario inputs and default build paths.
    @param scenario Persisted scenario whose three CSV inputs are passed to the CLI.
    @param run_id Database identifier used to create unique artifact filenames.
    @return A succeeded or failed normalized solver result.
    """
    output_dir = run_output_dir(root)
    output_dir.mkdir(parents=True, exist_ok=True)
    dispatch_path = output_dir / f"run_{run_id:06d}_dispatch.csv"
    summary_path = output_dir / f"run_{run_id:06d}_summary.json"

    # Remove stale files so only artifacts produced by this invocation are accepted.
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
    # Scalar values are persisted in PostgreSQL; the temporary JSON is not retained.
    summary_path.unlink(missing_ok=True)
    return SolverResult(
        "succeeded",
        display_path(root, dispatch_path),
        None,
        summary,
    )


def resolve_dispatch_path(root: Path, stored_path: str) -> Path | None:
    """Resolve and validate a persisted dispatch artifact path.

    Only paths contained by the configured run-output directory are accepted. This
    prevents stored values from exposing arbitrary server files.

    @param root Repository root used for relative stored paths.
    @param stored_path Database path to validate.
    @return The resolved managed path, or `None` when resolution or containment
    validation fails.
    """
    output_root = run_output_dir(root).resolve()
    candidate = Path(stored_path)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        resolved = candidate.resolve()
        # Database-controlled paths must remain below the managed artifact root.
        resolved.relative_to(output_root)
    except (OSError, ValueError):
        return None
    return resolved
