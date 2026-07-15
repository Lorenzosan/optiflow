from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from backend.app.models import Scenario
from backend.app.scenario_uploads import (
    MAX_SCENARIO_BYTES,
    MAX_SERIES_BYTES,
    managed_scenario_directory,
    scenario_storage_dir,
)


class ScenarioInputError(RuntimeError):
    """Stored scenario inputs cannot be read safely."""


@dataclass(frozen=True)
class ScenarioInputData:
    editable: bool
    scenario_csv: str
    prices_csv: str
    inflows_csv: str


def resolve_scenario_input_path(root: Path, stored_path: str) -> Path:
    candidate = Path(stored_path)
    if not candidate.is_absolute():
        candidate = root / candidate

    try:
        resolved = candidate.resolve()
        allowed_roots = (root.resolve(), scenario_storage_dir(root).resolve())
        if not any(_is_relative_to(resolved, allowed_root) for allowed_root in allowed_roots):
            raise ScenarioInputError("Scenario input path is outside managed storage")
    except OSError as error:
        raise ScenarioInputError("Scenario input path cannot be resolved") from error

    if not resolved.is_file():
        raise ScenarioInputError("Scenario input file is unavailable")
    return resolved


def read_scenario_inputs(root: Path, scenario: Scenario) -> ScenarioInputData:
    scenario_path = resolve_scenario_input_path(root, scenario.scenario_path)
    prices_path = resolve_scenario_input_path(root, scenario.prices_path)
    inflows_path = resolve_scenario_input_path(root, scenario.inflows_path)

    return ScenarioInputData(
        editable=managed_scenario_directory(
            root,
            scenario.scenario_path,
            scenario.prices_path,
            scenario.inflows_path,
        )
        is not None,
        scenario_csv=_read_utf8(scenario_path, "scenario", MAX_SCENARIO_BYTES),
        prices_csv=_read_utf8(prices_path, "prices", MAX_SERIES_BYTES),
        inflows_csv=_read_utf8(inflows_path, "inflows", MAX_SERIES_BYTES),
    )


def _read_utf8(path: Path, label: str, max_bytes: int) -> str:
    try:
        size = path.stat().st_size
        if size <= 0:
            raise ScenarioInputError(f"Stored {label} file is empty")
        if size > max_bytes:
            raise ScenarioInputError(f"Stored {label} file exceeds its size limit")
        return path.read_text(encoding="utf-8")
    except UnicodeError as error:
        raise ScenarioInputError(f"Stored {label} file is not valid UTF-8") from error
    except OSError as error:
        raise ScenarioInputError(f"Stored {label} file cannot be read") from error


def _is_relative_to(path: Path, root: Path) -> bool:
    try:
        path.relative_to(root)
        return True
    except ValueError:
        return False
