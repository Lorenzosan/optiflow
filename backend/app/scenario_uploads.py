from __future__ import annotations

import csv
from dataclasses import dataclass
from pathlib import Path
import os
import shutil
import subprocess
from uuid import uuid4

from fastapi import UploadFile

from backend.app.runner import display_path, solve_executable, truncate_error


SCENARIO_STORAGE_DIR_ENV = "OPTIFLOW_SCENARIO_STORAGE_DIR"
DEFAULT_SCENARIO_STORAGE_DIR = "data/scenarios"
MAX_SCENARIO_BYTES = 256 * 1024
MAX_SERIES_BYTES = 8 * 1024 * 1024
VALIDATION_TIMEOUT_SECONDS = 30


class ScenarioUploadError(ValueError):
    """Uploaded scenario input is malformed or invalid."""


class ScenarioValidatorError(RuntimeError):
    """The server-side scenario validator could not be executed."""


@dataclass(frozen=True)
class StoredScenarioFiles:
    name: str
    scenario_path: str
    prices_path: str
    inflows_path: str
    directory: Path


def scenario_storage_dir(root: Path) -> Path:
    configured = os.environ.get(SCENARIO_STORAGE_DIR_ENV, DEFAULT_SCENARIO_STORAGE_DIR)
    storage_dir = Path(configured).expanduser()
    if storage_dir.is_absolute():
        return storage_dir
    return root / storage_dir


def managed_scenario_directory(
    root: Path,
    scenario_path: str,
    prices_path: str,
    inflows_path: str,
) -> Path | None:
    storage_root = scenario_storage_dir(root).resolve()
    expected_names = ("scenario.csv", "prices.csv", "inflows.csv")
    resolved_paths: list[Path] = []
    for stored_path, expected_name in zip(
        (scenario_path, prices_path, inflows_path),
        expected_names,
        strict=True,
    ):
        candidate = Path(stored_path)
        if not candidate.is_absolute():
            candidate = root / candidate
        resolved = candidate.resolve()
        try:
            resolved.relative_to(storage_root)
        except ValueError:
            return None
        if resolved.name != expected_name:
            return None
        resolved_paths.append(resolved)

    directory = resolved_paths[0].parent
    if directory == storage_root or any(path.parent != directory for path in resolved_paths):
        return None
    return directory


async def write_upload(
    upload: UploadFile,
    target: Path,
    *,
    label: str,
    max_bytes: int,
) -> None:
    try:
        filename = upload.filename or ""
        if Path(filename).suffix.lower() != ".csv":
            raise ScenarioUploadError(f"{label} must be uploaded as a .csv file")

        contents = await upload.read(max_bytes + 1)
        if not contents:
            raise ScenarioUploadError(f"{label} file is empty")
        if len(contents) > max_bytes:
            raise ScenarioUploadError(f"{label} file exceeds the {max_bytes}-byte limit")

        try:
            target.write_bytes(contents)
        except OSError as error:
            raise ScenarioValidatorError(f"cannot store {label} file") from error
    finally:
        await upload.close()


def validate_scenario_inputs(
    root: Path,
    scenario_path: Path,
    prices_path: Path,
    inflows_path: Path,
) -> None:
    validator = solve_executable(root)
    if not validator.is_file():
        raise ScenarioValidatorError(f"scenario validator executable not found: {validator}")

    command = [
        str(validator),
        "--scenario",
        str(scenario_path),
        "--prices",
        str(prices_path),
        "--inflows",
        str(inflows_path),
        "--validate-only",
    ]
    try:
        completed = subprocess.run(
            command,
            cwd=root,
            text=True,
            capture_output=True,
            timeout=VALIDATION_TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise ScenarioValidatorError("scenario validation timed out") from error
    except OSError as error:
        raise ScenarioValidatorError("scenario validator could not be started") from error

    if completed.returncode != 0:
        error_text = completed.stderr or completed.stdout or "scenario validation failed"
        raise ScenarioUploadError(truncate_error(error_text))


def read_scenario_name(path: Path) -> str:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            header = next(reader, None)
            if header != ["key", "value"]:
                raise ScenarioUploadError("scenario file must use the header key,value")
            for row in reader:
                if len(row) == 2 and row[0].strip() == "scenario_name":
                    name = row[1].strip()
                    if not name:
                        raise ScenarioUploadError("scenario_name must not be empty")
                    if len(name) > 128:
                        raise ScenarioUploadError("scenario_name must be at most 128 characters")
                    return name
    except (OSError, UnicodeError) as error:
        raise ScenarioUploadError("scenario file must be readable UTF-8 CSV") from error

    raise ScenarioUploadError("scenario file is missing scenario_name")


def remove_scenario_directory(path: Path) -> None:
    shutil.rmtree(path, ignore_errors=True)


async def store_validated_scenario(
    root: Path,
    scenario_upload: UploadFile,
    prices_upload: UploadFile,
    inflows_upload: UploadFile,
) -> StoredScenarioFiles:
    storage_root = scenario_storage_dir(root)
    try:
        storage_root.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        raise ScenarioValidatorError("scenario storage directory is unavailable") from error

    token = uuid4().hex
    temporary_dir = storage_root / f".upload-{token}"
    final_dir = storage_root / token
    try:
        temporary_dir.mkdir()
        scenario_path = temporary_dir / "scenario.csv"
        prices_path = temporary_dir / "prices.csv"
        inflows_path = temporary_dir / "inflows.csv"

        await write_upload(
            scenario_upload,
            scenario_path,
            label="scenario",
            max_bytes=MAX_SCENARIO_BYTES,
        )
        await write_upload(
            prices_upload,
            prices_path,
            label="prices",
            max_bytes=MAX_SERIES_BYTES,
        )
        await write_upload(
            inflows_upload,
            inflows_path,
            label="inflows",
            max_bytes=MAX_SERIES_BYTES,
        )

        validate_scenario_inputs(root, scenario_path, prices_path, inflows_path)
        scenario_name = read_scenario_name(scenario_path)
        temporary_dir.replace(final_dir)
    except Exception:
        remove_scenario_directory(temporary_dir)
        remove_scenario_directory(final_dir)
        raise

    return StoredScenarioFiles(
        name=scenario_name,
        scenario_path=display_path(root, final_dir / "scenario.csv"),
        prices_path=display_path(root, final_dir / "prices.csv"),
        inflows_path=display_path(root, final_dir / "inflows.csv"),
        directory=final_dir,
    )
