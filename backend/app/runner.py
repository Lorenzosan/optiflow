from dataclasses import dataclass
from pathlib import Path
import os
import subprocess

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from backend.app.models import Scenario


SOLVE_EXECUTABLE_ENV = "OPTIFLOW_SOLVE_BIN"
RUN_OUTPUT_DIR_ENV = "OPTIFLOW_RUN_OUTPUT_DIR"
SOLVE_TIMEOUT_SECONDS_ENV = "OPTIFLOW_SOLVE_TIMEOUT_SECONDS"
DEFAULT_SOLVE_TIMEOUT_SECONDS = 600
DEFAULT_RUN_OUTPUT_DIR = "build/api-runs"
MAX_ERROR_MESSAGE_LENGTH = 4000


class RunSummaryData(BaseModel):
    model_config = ConfigDict(extra="forbid", frozen=True, strict=True)

    cumulative_profit: float = Field(allow_inf_nan=False)
    export_energy_mwh: float = Field(ge=0.0, allow_inf_nan=False)
    import_energy_mwh: float = Field(ge=0.0, allow_inf_nan=False)
    final_reservoir_volume: float = Field(allow_inf_nan=False)
    final_battery_soc: float = Field(allow_inf_nan=False)
    solve_seconds: float = Field(ge=0.0, allow_inf_nan=False)
    simulation_seconds: float = Field(ge=0.0, allow_inf_nan=False)
    turbine_steps: int = Field(ge=0)
    pump_steps: int = Field(ge=0)
    spill_steps: int = Field(ge=0)
    battery_charge_steps: int = Field(ge=0)
    battery_discharge_steps: int = Field(ge=0)
    wait_steps: int = Field(ge=0)


@dataclass(frozen=True)
class SolverResult:
    status: str
    output_dispatch_path: str | None
    error_message: str | None
    summary: RunSummaryData | None


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


def read_summary(path: Path) -> RunSummaryData:
    try:
        return RunSummaryData.model_validate_json(path.read_text())
    except (OSError, UnicodeError) as error:
        raise ValueError(f"cannot read summary JSON: {error}") from error
    except ValidationError as error:
        details = []
        for item in error.errors(include_url=False):
            location = ".".join(str(part) for part in item["loc"])
            detail = f"{location}: {item['msg']}" if location else item["msg"]
            details.append(detail)
        raise ValueError("; ".join(details)) from error


def remove_artifact(path: Path) -> None:
    try:
        path.unlink(missing_ok=True)
    except OSError:
        pass


def failed_result(message: str, output_path: Path, summary_path: Path) -> SolverResult:
    remove_artifact(output_path)
    remove_artifact(summary_path)
    return SolverResult(
        status="failed",
        output_dispatch_path=None,
        error_message=truncate_error(message),
        summary=None,
    )


def run_solver(root: Path, scenario: Scenario, run_id: int) -> SolverResult:
    missing_inputs = missing_input_error(root, scenario)
    if missing_inputs is not None:
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=missing_inputs,
            summary=None,
        )

    solver = solve_executable(root)
    if not solver.is_file():
        return SolverResult(
            status="failed",
            output_dispatch_path=None,
            error_message=f"Solver executable not found: {solver}",
            summary=None,
        )

    output_dir = run_output_dir(root)
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / f"run_{run_id:06d}_dispatch.csv"
    summary_path = output_dir / f"run_{run_id:06d}_summary.json"
    remove_artifact(output_path)
    remove_artifact(summary_path)

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
        "--summary-output",
        str(summary_path),
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
        return failed_result(
            f"Solver timed out after {exc.timeout} seconds.",
            output_path,
            summary_path,
        )

    if completed.returncode != 0:
        error_text = completed.stderr or completed.stdout or f"Solver exited with code {completed.returncode}."
        return failed_result(error_text, output_path, summary_path)

    if not output_path.is_file():
        return failed_result(
            "Solver finished successfully but did not write the dispatch CSV.",
            output_path,
            summary_path,
        )
    if not summary_path.is_file():
        return failed_result(
            "Solver finished successfully but did not write the summary JSON.",
            output_path,
            summary_path,
        )

    try:
        summary = read_summary(summary_path)
    except ValueError as error:
        return failed_result(
            f"Solver summary JSON is invalid: {error}",
            output_path,
            summary_path,
        )

    remove_artifact(summary_path)
    return SolverResult(
        status="succeeded",
        output_dispatch_path=display_path(root, output_path),
        error_message=None,
        summary=summary,
    )
