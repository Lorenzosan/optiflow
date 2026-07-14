#!/usr/bin/env python3
"""Run hand-computable hydro dispatch cases through the CLI and validator."""

from __future__ import annotations

import argparse
import csv
import json
import math
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DISPATCH_HEADER = [
    "time_index",
    "timestamp_utc",
    "price",
    "natural_inflow",
    "reservoir_volume",
    "turbine_flow",
    "spill_flow",
    "pump_flow",
    "next_reservoir_volume",
    "net_power",
    "reward",
    "cumulative_profit",
]


@dataclass(frozen=True)
class OracleCase:
    directory: str
    expected_rows: tuple[dict[str, float | str], ...]
    expected_summary: dict[str, float | int]


CASES = (
    OracleCase(
        directory="pump_generate_cycle",
        expected_rows=(
            {
                "time_index": 0,
                "timestamp_utc": "2027-01-04T00:00:00Z",
                "price": -20.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 0.0,
                "turbine_flow": 0.0,
                "spill_flow": 0.0,
                "pump_flow": 10.0,
                "next_reservoir_volume": 10.0,
                "net_power": -5.0,
                "reward": 90.0,
                "cumulative_profit": 90.0,
            },
            {
                "time_index": 1,
                "timestamp_utc": "2027-01-04T01:00:00Z",
                "price": 100.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 10.0,
                "turbine_flow": 10.0,
                "spill_flow": 0.0,
                "pump_flow": 0.0,
                "next_reservoir_volume": 0.0,
                "net_power": 3.2,
                "reward": 313.6,
                "cumulative_profit": 403.6,
            },
        ),
        expected_summary={
            "cumulative_profit": 403.6,
            "export_energy_mwh": 3.2,
            "import_energy_mwh": 5.0,
            "final_reservoir_volume": 0.0,
            "turbine_steps": 1,
            "pump_steps": 1,
            "spill_steps": 0,
            "wait_steps": 0,
        },
    ),
    OracleCase(
        directory="break_even_below",
        expected_rows=(
            {
                "time_index": 0,
                "timestamp_utc": "2027-01-04T00:00:00Z",
                "price": 30.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 0.0,
                "turbine_flow": 0.0,
                "spill_flow": 0.0,
                "pump_flow": 0.0,
                "next_reservoir_volume": 0.0,
                "net_power": 0.0,
                "reward": 0.0,
                "cumulative_profit": 0.0,
            },
            {
                "time_index": 1,
                "timestamp_utc": "2027-01-04T01:00:00Z",
                "price": 50.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 0.0,
                "turbine_flow": 0.0,
                "spill_flow": 0.0,
                "pump_flow": 0.0,
                "next_reservoir_volume": 0.0,
                "net_power": 0.0,
                "reward": 0.0,
                "cumulative_profit": 0.0,
            },
        ),
        expected_summary={
            "cumulative_profit": 0.0,
            "export_energy_mwh": 0.0,
            "import_energy_mwh": 0.0,
            "final_reservoir_volume": 0.0,
            "turbine_steps": 0,
            "pump_steps": 0,
            "spill_steps": 0,
            "wait_steps": 2,
        },
    ),
    OracleCase(
        directory="break_even_above",
        expected_rows=(
            {
                "time_index": 0,
                "timestamp_utc": "2027-01-04T00:00:00Z",
                "price": 30.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 0.0,
                "turbine_flow": 0.0,
                "spill_flow": 0.0,
                "pump_flow": 10.0,
                "next_reservoir_volume": 10.0,
                "net_power": -5.0,
                "reward": -160.0,
                "cumulative_profit": -160.0,
            },
            {
                "time_index": 1,
                "timestamp_utc": "2027-01-04T01:00:00Z",
                "price": 60.0,
                "natural_inflow": 0.0,
                "reservoir_volume": 10.0,
                "turbine_flow": 10.0,
                "spill_flow": 0.0,
                "pump_flow": 0.0,
                "next_reservoir_volume": 0.0,
                "net_power": 3.2,
                "reward": 185.6,
                "cumulative_profit": 25.6,
            },
        ),
        expected_summary={
            "cumulative_profit": 25.6,
            "export_energy_mwh": 3.2,
            "import_energy_mwh": 5.0,
            "final_reservoir_volume": 0.0,
            "turbine_steps": 1,
            "pump_steps": 1,
            "spill_steps": 0,
            "wait_steps": 0,
        },
    ),
    OracleCase(
        directory="forced_spill",
        expected_rows=(
            {
                "time_index": 0,
                "timestamp_utc": "2027-01-04T00:00:00Z",
                "price": -10.0,
                "natural_inflow": 5.0,
                "reservoir_volume": 8.0,
                "turbine_flow": 0.0,
                "spill_flow": 3.0,
                "pump_flow": 0.0,
                "next_reservoir_volume": 10.0,
                "net_power": 0.0,
                "reward": 0.0,
                "cumulative_profit": 0.0,
            },
        ),
        expected_summary={
            "cumulative_profit": 0.0,
            "export_energy_mwh": 0.0,
            "import_energy_mwh": 0.0,
            "final_reservoir_volume": 10.0,
            "turbine_steps": 0,
            "pump_steps": 0,
            "spill_steps": 1,
            "wait_steps": 0,
        },
    ),
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--solve", required=True, type=Path)
    parser.add_argument("--validator", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    return parser.parse_args()


def require_close(label: str, actual: float, expected: float, tolerance: float = 1.0e-9) -> None:
    if not math.isclose(actual, expected, rel_tol=tolerance, abs_tol=tolerance):
        raise AssertionError(f"{label}: expected {expected}, got {actual}")


def read_dispatch(path: Path) -> list[dict[str, float | str]]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != DISPATCH_HEADER:
            raise AssertionError(f"{path}: unexpected dispatch header {reader.fieldnames}")
        rows: list[dict[str, float | str]] = []
        for row in reader:
            parsed: dict[str, float | str] = {
                key: float(value)
                for key, value in row.items()
                if key != "timestamp_utc"
            }
            parsed["timestamp_utc"] = row["timestamp_utc"]
            rows.append(parsed)
        return rows


def assert_no_negative_zero_csv(path: Path) -> None:
    with path.open(newline="") as handle:
        for row_index, row in enumerate(csv.reader(handle)):
            if row_index == 0:
                continue
            for column_index, value in enumerate(row):
                try:
                    numeric = float(value)
                except ValueError:
                    continue
                if numeric == 0.0 and value.lstrip().startswith("-"):
                    raise AssertionError(
                        f"{path}: negative zero at row {row_index + 1}, column {column_index + 1}"
                    )


def assert_no_negative_zero_json(path: Path) -> None:
    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        if ": -0" in line:
            raise AssertionError(f"{path}: negative zero at line {line_number}")


def assert_dispatch(case: OracleCase, rows: list[dict[str, float]]) -> None:
    if len(rows) != len(case.expected_rows):
        raise AssertionError(
            f"{case.directory}: expected {len(case.expected_rows)} dispatch rows, got {len(rows)}"
        )
    for row_index, (actual, expected) in enumerate(zip(rows, case.expected_rows, strict=True)):
        for field, expected_value in expected.items():
            if isinstance(expected_value, str):
                if actual[field] != expected_value:
                    raise AssertionError(
                        f"{case.directory} row {row_index} field {field}: "
                        f"expected {expected_value}, got {actual[field]}"
                    )
                continue
            require_close(
                f"{case.directory} row {row_index} field {field}",
                float(actual[field]),
                float(expected_value),
            )


def assert_summary(case: OracleCase, summary: dict[str, object]) -> None:
    expected_keys = set(case.expected_summary) | {"solve_seconds", "simulation_seconds"}
    if set(summary) != expected_keys:
        raise AssertionError(
            f"{case.directory}: expected summary keys {sorted(expected_keys)}, got {sorted(summary)}"
        )
    for field, expected_value in case.expected_summary.items():
        actual = summary[field]
        if isinstance(expected_value, int):
            if actual != expected_value:
                raise AssertionError(
                    f"{case.directory} summary {field}: expected {expected_value}, got {actual}"
                )
        else:
            require_close(f"{case.directory} summary {field}", float(actual), expected_value)
    for field in ("solve_seconds", "simulation_seconds"):
        value = float(summary[field])
        if not math.isfinite(value) or value < 0.0:
            raise AssertionError(f"{case.directory} summary {field} must be finite and nonnegative")


def run_case(args: argparse.Namespace, case: OracleCase) -> None:
    fixture_dir = args.source_dir / "tests" / "fixtures" / case.directory
    case_output_dir = args.output_dir / case.directory
    shutil.rmtree(case_output_dir, ignore_errors=True)
    case_output_dir.mkdir(parents=True)
    dispatch_path = case_output_dir / "dispatch.csv"
    summary_path = case_output_dir / "summary.json"
    stdout_path = case_output_dir / "stdout.txt"

    command = [
        str(args.solve),
        "--scenario", str(fixture_dir / "scenario.csv"),
        "--prices", str(fixture_dir / "prices.csv"),
        "--inflows", str(fixture_dir / "inflows.csv"),
        "--output", str(dispatch_path),
        "--summary-output", str(summary_path),
    ]
    completed = subprocess.run(command, check=False, capture_output=True, text=True)
    stdout_path.write_text(completed.stdout)
    if completed.returncode != 0:
        raise AssertionError(
            f"{case.directory}: optimizer failed with code {completed.returncode}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )

    validator = subprocess.run(
        [
            sys.executable,
            str(args.validator),
            "--scenario", str(fixture_dir / "scenario.csv"),
            "--prices", str(fixture_dir / "prices.csv"),
            "--inflows", str(fixture_dir / "inflows.csv"),
            "--dispatch", str(dispatch_path),
            "--stdout", str(stdout_path),
        ],
        check=False,
        capture_output=True,
        text=True,
    )
    if validator.returncode != 0:
        raise AssertionError(
            f"{case.directory}: validator failed with code {validator.returncode}\n"
            f"stdout:\n{validator.stdout}\nstderr:\n{validator.stderr}"
        )

    assert_no_negative_zero_csv(dispatch_path)
    assert_no_negative_zero_json(summary_path)
    assert_dispatch(case, read_dispatch(dispatch_path))
    assert_summary(case, json.loads(summary_path.read_text()))


def main() -> int:
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for case in CASES:
        run_case(args, case)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
