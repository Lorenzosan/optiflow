from __future__ import annotations

from dataclasses import dataclass, replace
from hashlib import sha256
from pathlib import Path
import csv

from backend.app.models import Scenario


RESULT_SCHEMA_VERSION = 1
HASH_CHUNK_BYTES = 1024 * 1024


class ProvenanceError(ValueError):
    """Run provenance cannot be collected from the configured execution inputs."""


@dataclass(frozen=True)
class RunProvenanceData:
    result_schema_version: int
    scenario_sha256: str
    prices_sha256: str
    inflows_sha256: str
    solver_sha256: str
    dispatch_sha256: str | None
    horizon_steps: int
    reservoir_volume_grid_points: int
    turbine_flow_steps: int
    pump_flow_steps: int
    spill_flow_steps: int

    def with_dispatch_sha256(self, dispatch_sha256: str) -> RunProvenanceData:
        return replace(self, dispatch_sha256=dispatch_sha256)


def sha256_file(path: Path) -> str:
    digest = sha256()
    try:
        with path.open("rb") as handle:
            while chunk := handle.read(HASH_CHUNK_BYTES):
                digest.update(chunk)
    except OSError as error:
        raise ProvenanceError(f"cannot read {path}: {error}") from error
    return digest.hexdigest()


def scenario_grid_configuration(path: Path) -> tuple[int, int, int, int]:
    required_keys = (
        "reservoir_volume_grid_points",
        "turbine_flow_steps",
        "pump_flow_steps",
        "spill_flow_steps",
    )
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != ["key", "value"]:
                raise ProvenanceError(f"{path} must use the header key,value")
            values: dict[str, str] = {}
            for row_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    raise ProvenanceError(
                        f"scenario row {row_number} in {path} must contain two columns"
                    )
                key = row[0].strip()
                if not key:
                    raise ProvenanceError(
                        f"scenario row {row_number} in {path} has an empty key"
                    )
                if key in values:
                    raise ProvenanceError(f"{path} contains duplicate key {key}")
                values[key] = row[1].strip()
    except (OSError, UnicodeError) as error:
        raise ProvenanceError(f"cannot read {path} as UTF-8 CSV: {error}") from error

    parsed: list[int] = []
    for key in required_keys:
        try:
            value = int(values[key])
        except KeyError as error:
            raise ProvenanceError(f"{path} is missing {key}") from error
        except ValueError as error:
            raise ProvenanceError(f"{key} in {path} must be an integer") from error
        if value <= 0:
            raise ProvenanceError(f"{key} in {path} must be positive")
        parsed.append(value)
    return parsed[0], parsed[1], parsed[2], parsed[3]


def series_row_count(path: Path, expected_header: list[str]) -> int:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != expected_header:
                expected = ",".join(expected_header)
                raise ProvenanceError(f"{path} must use the header {expected}")
            row_count = 0
            for row_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    raise ProvenanceError(
                        f"series row {row_number} in {path} must contain two columns"
                    )
                row_count += 1
            return row_count
    except (OSError, UnicodeError) as error:
        raise ProvenanceError(f"cannot read {path} as UTF-8 CSV: {error}") from error


def collect_run_provenance(
    root: Path,
    scenario: Scenario,
    solver_path: Path,
) -> RunProvenanceData:
    scenario_path = root / scenario.scenario_path
    prices_path = root / scenario.prices_path
    inflows_path = root / scenario.inflows_path

    prices_steps = series_row_count(prices_path, ["timestamp_utc", "price"])
    inflows_steps = series_row_count(
        inflows_path,
        ["timestamp_utc", "natural_inflow"],
    )
    if prices_steps != inflows_steps:
        raise ProvenanceError(
            "price and inflow series have different row counts: "
            f"{prices_steps} and {inflows_steps}"
        )

    (
        reservoir_volume_grid_points,
        turbine_flow_steps,
        pump_flow_steps,
        spill_flow_steps,
    ) = scenario_grid_configuration(scenario_path)

    return RunProvenanceData(
        result_schema_version=RESULT_SCHEMA_VERSION,
        scenario_sha256=sha256_file(scenario_path),
        prices_sha256=sha256_file(prices_path),
        inflows_sha256=sha256_file(inflows_path),
        solver_sha256=sha256_file(solver_path),
        dispatch_sha256=None,
        horizon_steps=prices_steps,
        reservoir_volume_grid_points=reservoir_volume_grid_points,
        turbine_flow_steps=turbine_flow_steps,
        pump_flow_steps=pump_flow_steps,
        spill_flow_steps=spill_flow_steps,
    )
