"""@file
@brief Stage, validate, and publish uploaded scenario CSV files safely.

Uploads are written into a server-generated temporary directory, validated by
the C++ CLI, and atomically renamed into managed storage only after successful
validation and scenario-name extraction.
"""

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


## @brief Environment variable overriding managed uploaded-scenario storage.
SCENARIO_STORAGE_DIR_ENV = "OPTIFLOW_SCENARIO_STORAGE_DIR"

## @brief Repository-relative default for uploaded scenario directories.
DEFAULT_SCENARIO_STORAGE_DIR = "data/scenarios"

## @brief Maximum accepted size of the scalar scenario CSV.
MAX_SCENARIO_BYTES = 256 * 1024

## @brief Maximum accepted size of each timestamped series CSV.
MAX_SERIES_BYTES = 8 * 1024 * 1024

## @brief Maximum wall-clock duration allowed for input validation.
VALIDATION_TIMEOUT_SECONDS = 30


class ScenarioUploadError(ValueError):
    """Report malformed or optimizer-invalid uploaded scenario data."""


class ScenarioValidatorError(RuntimeError):
    """Report unavailable storage or validator infrastructure."""


@dataclass(frozen=True)
class StoredScenarioFiles:
    """Describe one validated scenario published into managed storage.

    Paths are formatted for database persistence and `directory` is retained for
    transactional cleanup when later database operations fail.
    """
    name: str
    scenario_path: str
    prices_path: str
    inflows_path: str
    directory: Path


def scenario_storage_dir(root: Path) -> Path:
    """Resolve the managed root for uploaded scenarios.

    @param root Repository root used for relative configuration values.
    @return Absolute or repository-relative path selected through
    `OPTIFLOW_SCENARIO_STORAGE_DIR`.
    """
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
    """Recognize whether three stored paths form one managed scenario directory.

    The files must be named `scenario.csv`, `prices.csv`, and `inflows.csv`, reside
    under the configured storage root, and share one non-root parent directory.

    @param root Repository root used for relative stored paths.
    @param scenario_path Persisted scalar scenario path.
    @param prices_path Persisted price-series path.
    @param inflows_path Persisted inflow-series path.
    @return The managed directory, or `None` for bundled, malformed, or unsafe paths.
    """
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
    """Validate basic upload metadata and write one bounded CSV file.

    The upload is always closed, including when validation or storage fails.

    @param upload FastAPI upload stream.
    @param target Server-generated destination path.
    @param label Human-readable input name used in errors.
    @param max_bytes Maximum accepted payload size.
    @throws ScenarioUploadError When the filename, size, or content is invalid.
    @throws ScenarioValidatorError When the server cannot persist the file.
    """
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
    """Validate a staged scenario with the compiled C++ CLI.

    The command runs with `--validate-only`, so no dispatch artifact is produced.
    Optimizer validation errors are returned as upload errors, while execution
    infrastructure errors remain server errors.

    @param root Repository root and subprocess working directory.
    @param scenario_path Staged scalar scenario CSV.
    @param prices_path Staged price CSV.
    @param inflows_path Staged natural-inflow CSV.
    @throws ScenarioUploadError When the optimizer rejects the inputs.
    @throws ScenarioValidatorError When the validator is missing, times out, or
    cannot start.
    """
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
    """Read and validate `scenario_name` from a staged scenario CSV.

    @param path Scalar scenario CSV path.
    @return Nonempty scenario name of at most 128 characters.
    @throws ScenarioUploadError When the file, header, encoding, or name is invalid.
    """
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
    """Remove a managed scenario directory without propagating cleanup errors.

    @param path Directory to remove recursively.
    """
    shutil.rmtree(path, ignore_errors=True)


async def store_validated_scenario(
    root: Path,
    scenario_upload: UploadFile,
    prices_upload: UploadFile,
    inflows_upload: UploadFile,
) -> StoredScenarioFiles:
    """Stage, validate, and publish one uploaded scenario atomically.

    All three uploads are stored under a random server-generated token. Any error
    removes both temporary and final directories. Successful publication returns
    paths ready for database persistence.

    @param root Repository root used for configuration and validator execution.
    @param scenario_upload Scalar scenario CSV upload.
    @param prices_upload Electricity-price CSV upload.
    @param inflows_upload Natural-inflow CSV upload.
    @return Published managed file paths and parsed scenario name.
    @throws ScenarioUploadError When user-provided data is invalid.
    @throws ScenarioValidatorError When storage or validation infrastructure fails.
    """
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

        # Publish only after all inputs pass C++ validation and metadata parsing.
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
