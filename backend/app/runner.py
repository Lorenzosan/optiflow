from dataclasses import dataclass
from pathlib import Path
import os
import subprocess

from backend.app.models import Scenario


SOLVE_EXECUTABLE_ENV = "OPTIFLOW_SOLVE_BIN"
RUN_OUTPUT_DIR_ENV = "OPTIFLOW_RUN_OUTPUT_DIR"
SOLVE_TIMEOUT_SECONDS_ENV = "OPTIFLOW_SOLVE_TIMEOUT_SECONDS"
DEFAULT_SOLVE_TIMEOUT_SECONDS = 600
DEFAULT_RUN_OUTPUT_DIR = "build/api-runs"
MAX_ERROR_MESSAGE_LENGTH = 4000


@dataclass(frozen=True)
class SolverResult:
    status: str
    output_dispatch_path: str | None
    error_message: str | None


def solve_executable(root: Path) -> Path:
    configured = os.environ.get(SOLVE_EXECUTABLE_ENV)
    if configured:
        return Path(configured).expanduser().resolve()
    return root / "build" / "apps" / "solve_cli" / "optiflow_solve"


def run_output_dir(root: Path) -> Path:
    configured = os.environ.get(RUN_OUTPUT_DIR_ENV, DEFAULT_RUN_OUTPUT_DIR)
    output_dir = Path(configured).expanduser()
    if output_dir.is_absolute():
        return output_dir
    return root / output_dir


def solve_timeout_seconds() -> int:
    configured = os.environ.get(SOLVE_TIMEOUT_SECONDS_ENV)
    if configured is None:
        return DEFAULT_SOLVE_TIMEOUT_SECONDS
    return int(configured)


def display_path(root: Path, path: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return str(path)


def resolve_dispatch_path(root: Path, stored_path: str) -> Path | None:
    try:
        output_dir = run_output_dir(root).resolve()
        candidate = Path(stored_path).expanduser()
        if not candidate.is_absolute():
            candidate = root / candidate
        candidate = candidate.resolve()
        candidate.relative_to(output_dir)
    except (OSError, RuntimeError, ValueError):
        return None
    return candidate


def truncate_error(message: str) -> str:
    stripped = message.strip()
    if len(stripped) <= MAX_ERROR_MESSAGE_LENGTH:
        return stripped
    return stripped[: MAX_ERROR_MESSAGE_LENGTH - 3] + "..."


def missing_input_error(root: Path, scenario: Scenario) -> str | None:
    missing_paths = [
        relative_path
        for relative_path in (scenario.scenario_path, scenario.prices_path, scenario.inflows_path)
        if not (root / relative_path).is_file()
    ]
    if not missing_paths:
        return None
    return "Missing input file(s): " + ", ".join(missing_paths)


def run_solver(root: Path, scenario: Scenario, run_id: int) -> SolverResult:
    missing_inputs = missing_input_error(root, scenario)
    if missing_inputs is not None:
        return SolverResult(status="failed", output_dispatch_path=None, error_message=missing_inputs)

    solver = solve_executable(root)
    if not solver.is_file():
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=f"Solver executable not found: {solver}",
        )

    output_dir = run_output_dir(root)
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"run_{run_id:06d}_dispatch.csv"

    command = [
        str(solver),
        "--scenario",
        scenario.scenario_path,
        "--prices",
        scenario.prices_path,
        "--inflows",
        scenario.inflows_path,
        "--output",
        str(output_path),
    ]

    try:
        completed = subprocess.run(
            command,
            cwd=root,
            text=True,
            capture_output=True,
            timeout=solve_timeout_seconds(),
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(f"Solver timed out after {exc.timeout} seconds."),
        )

    if completed.returncode != 0:
        error_text = completed.stderr or completed.stdout or f"Solver exited with code {completed.returncode}."
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=truncate_error(error_text),
        )

    if not output_path.is_file():
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message="Solver finished successfully but did not write the dispatch CSV.",
        )

    return SolverResult(
        status="succeeded",
        output_dispatch_path=display_path(root, output_path),
        error_message=None,
    )
