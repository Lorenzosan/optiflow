#!/usr/bin/env python3
"""Compare OptiFlow results across nested state and action resolutions."""

from __future__ import annotations

import argparse
import csv
import json
import math
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_RESOLUTIONS = ("9,3,3,3", "17,5,5,5", "33,9,9,9")
RESOLUTION_KEYS = (
    "reservoir_volume_grid_points",
    "turbine_flow_steps",
    "spill_flow_steps",
    "pump_flow_steps",
)
SUMMARY_FIELDS = (
    "net_operating_cashflow",
    "export_energy_mwh",
    "import_energy_mwh",
    "final_reservoir_volume",
    "solve_seconds",
    "simulation_seconds",
    "turbine_steps",
    "pump_steps",
    "spill_steps",
    "wait_steps",
)


class AnalysisError(RuntimeError):
    pass


@dataclass(frozen=True)
class Resolution:
    reservoir_points: int
    turbine_steps: int
    spill_steps: int
    pump_steps: int

    @property
    def label(self) -> str:
        return (
            f"r{self.reservoir_points}_t{self.turbine_steps}_"
            f"s{self.spill_steps}_p{self.pump_steps}"
        )

    def values(self) -> tuple[int, int, int, int]:
        return (
            self.reservoir_points,
            self.turbine_steps,
            self.spill_steps,
            self.pump_steps,
        )


def fail(message: str) -> None:
    raise AnalysisError(message)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Compare one scenario across nested numerical resolutions."
    )
    parser.add_argument("--solve", required=True, type=Path)
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--prices", required=True, type=Path)
    parser.add_argument("--inflows", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--summary-output", type=Path)
    parser.add_argument(
        "--resolution",
        action="append",
        metavar="R,T,S,P",
        help=(
            "Reservoir points and turbine, spill, and pump steps. "
            "Repeat from coarse to fine."
        ),
    )
    parser.add_argument("--timeout-seconds", type=float, default=600.0)
    return parser.parse_args()


def parse_resolution(text: str) -> Resolution:
    parts = [part.strip() for part in text.split(",")]
    if len(parts) != 4:
        fail(f"resolution {text!r} must contain four comma-separated integers")
    try:
        values = tuple(int(part) for part in parts)
    except ValueError as error:
        raise AnalysisError(
            f"resolution {text!r} must contain four comma-separated integers"
        ) from error
    if any(value <= 0 for value in values):
        fail(f"resolution {text!r} must contain positive integers")
    if values[0] < 2:
        fail(f"resolution {text!r} must use at least two reservoir points")
    return Resolution(*values)


def nested_axis(coarse: int, fine: int) -> bool:
    if fine < coarse:
        return False
    return coarse == 1 or (fine > 1 and (fine - 1) % (coarse - 1) == 0)


def validate_resolutions(resolutions: list[Resolution]) -> None:
    if len(resolutions) < 2:
        fail("at least two resolutions are required")
    if len(set(resolutions)) != len(resolutions):
        fail("resolutions must be unique")

    axis_names = ("reservoir points", "turbine steps", "spill steps", "pump steps")
    for coarse, fine in zip(resolutions, resolutions[1:]):
        if fine.reservoir_points <= coarse.reservoir_points:
            fail("reservoir points must increase from coarse to fine")
        for name, coarse_value, fine_value in zip(
            axis_names, coarse.values(), fine.values(), strict=True
        ):
            if not nested_axis(coarse_value, fine_value):
                fail(
                    f"{name} are not nested between {coarse.label} and {fine.label}: "
                    f"{coarse_value} -> {fine_value}"
                )


def read_scenario(path: Path) -> tuple[list[tuple[str, str]], dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != ["key", "value"]:
                fail(f"{path}: expected header key,value")
            rows: list[tuple[str, str]] = []
            params: dict[str, str] = {}
            for line_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    fail(f"{path}:{line_number}: expected two columns")
                key, value = row[0].strip(), row[1].strip()
                if not key:
                    fail(f"{path}:{line_number}: empty scenario key")
                if key in params:
                    fail(f"{path}:{line_number}: duplicate scenario key {key}")
                rows.append((key, value))
                params[key] = value
    except (OSError, UnicodeError) as error:
        raise AnalysisError(f"cannot read scenario {path}: {error}") from error

    missing = [key for key in RESOLUTION_KEYS if key not in params]
    if missing:
        fail(f"{path}: missing solver parameters: {', '.join(missing)}")
    return rows, params


def write_variant(
    rows: list[tuple[str, str]],
    base_name: str,
    resolution: Resolution,
    path: Path,
) -> None:
    overrides = dict(zip(RESOLUTION_KEYS, resolution.values(), strict=True))
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["key", "value"])
        for key, value in rows:
            if key == "scenario_name":
                value = f"{base_name}_{resolution.label}"
            elif key in overrides:
                value = str(overrides[key])
            writer.writerow([key, value])


def scenario_float(params: dict[str, str], key: str) -> float:
    try:
        value = float(params[key])
    except (KeyError, ValueError) as error:
        raise AnalysisError(f"scenario parameter {key} must be numeric") from error
    if not math.isfinite(value) or value < 0.0:
        fail(f"scenario parameter {key} must be finite and nonnegative")
    return value


def effective_axis_size(maximum: float, steps: int) -> int:
    return 1 if maximum == 0.0 or steps == 1 else steps


def count_actions(params: dict[str, str], resolution: Resolution) -> int:
    turbine = effective_axis_size(
        scenario_float(params, "turbine_max_flow"), resolution.turbine_steps
    )
    spill = effective_axis_size(
        scenario_float(params, "spill_max_flow"), resolution.spill_steps
    )
    pump = effective_axis_size(
        scenario_float(params, "pump_max_flow"), resolution.pump_steps
    )
    return turbine * spill + pump - 1


def read_summary(path: Path) -> dict[str, float | int]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise AnalysisError(f"cannot read solver summary {path}: {error}") from error
    if not isinstance(payload, dict):
        fail(f"{path}: solver summary must be a JSON object")

    result: dict[str, float | int] = {}
    integer_fields = {"turbine_steps", "pump_steps", "spill_steps", "wait_steps"}
    for field in SUMMARY_FIELDS:
        value = payload.get(field)
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            fail(f"{path}: summary field {field} must be numeric")
        if not math.isfinite(float(value)):
            fail(f"{path}: summary field {field} must be finite")
        if field in integer_fields and not isinstance(value, int):
            fail(f"{path}: summary field {field} must be an integer")
        result[field] = value if field in integer_fields else float(value)
    return result


def run_case(
    args: argparse.Namespace,
    rows: list[tuple[str, str]],
    params: dict[str, str],
    resolution: Resolution,
) -> dict[str, object]:
    scenario_path = args.output_dir / f"{resolution.label}_scenario.csv"
    dispatch_path = args.output_dir / f"{resolution.label}_dispatch.csv"
    summary_path = args.output_dir / f"{resolution.label}_summary.json"
    write_variant(
        rows,
        params.get("scenario_name", args.scenario.stem),
        resolution,
        scenario_path,
    )

    command = [
        str(args.solve),
        "--scenario", str(scenario_path),
        "--prices", str(args.prices),
        "--inflows", str(args.inflows),
        "--output", str(dispatch_path),
        "--summary-output", str(summary_path),
    ]
    try:
        completed = subprocess.run(
            command,
            text=True,
            capture_output=True,
            timeout=args.timeout_seconds,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise AnalysisError(
            f"resolution {resolution.label} exceeded {args.timeout_seconds:g} seconds"
        ) from error
    if completed.returncode != 0:
        message = (completed.stderr or completed.stdout).strip()
        fail(f"resolution {resolution.label} failed: {message}")
    if not dispatch_path.is_file():
        fail(f"resolution {resolution.label} did not create a dispatch artifact")

    return {
        "case": resolution.label,
        "reservoir_grid_points": resolution.reservoir_points,
        "turbine_flow_steps": resolution.turbine_steps,
        "spill_flow_steps": resolution.spill_steps,
        "pump_flow_steps": resolution.pump_steps,
        "action_count": count_actions(params, resolution),
        **read_summary(summary_path),
        "scenario_file": scenario_path.as_posix(),
        "dispatch_file": dispatch_path.as_posix(),
        "summary_file": summary_path.as_posix(),
    }


def add_finest_deltas(results: list[dict[str, object]]) -> None:
    finest = results[-1]
    comparisons = (
        ("net_operating_cashflow", "cashflow_delta_vs_finest"),
        ("export_energy_mwh", "export_delta_vs_finest"),
        ("import_energy_mwh", "import_delta_vs_finest"),
        ("final_reservoir_volume", "final_reservoir_delta_vs_finest"),
    )
    for result in results:
        for field, delta_field in comparisons:
            result[delta_field] = float(result[field]) - float(finest[field])


FIELDNAMES = (
    "case",
    "reservoir_grid_points",
    "turbine_flow_steps",
    "spill_flow_steps",
    "pump_flow_steps",
    "action_count",
    "net_operating_cashflow",
    "cashflow_delta_vs_finest",
    "export_energy_mwh",
    "export_delta_vs_finest",
    "import_energy_mwh",
    "import_delta_vs_finest",
    "final_reservoir_volume",
    "final_reservoir_delta_vs_finest",
    "solve_seconds",
    "simulation_seconds",
    "turbine_steps",
    "pump_steps",
    "spill_steps",
    "wait_steps",
    "scenario_file",
    "dispatch_file",
    "summary_file",
)


def write_csv(results: list[dict[str, object]], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES)
        writer.writeheader()
        writer.writerows(results)


def print_table(results: list[dict[str, object]], output_path: Path) -> None:
    headers = ("Resolution", "Actions", "Cashflow", "Delta finest", "Final res.", "Solve s")
    table = [
        (
            str(row["case"]),
            str(row["action_count"]),
            f"{float(row['net_operating_cashflow']):.6g}",
            f"{float(row['cashflow_delta_vs_finest']):.6g}",
            f"{float(row['final_reservoir_volume']):.6g}",
            f"{float(row['solve_seconds']):.4f}",
        )
        for row in results
    ]
    widths = [
        max(len(headers[index]), *(len(row[index]) for row in table))
        for index in range(len(headers))
    ]
    print("Resolution sensitivity")
    print("Deltas are measured against the finest listed resolution.")
    print("Cashflow is not assumed to improve monotonically when interpolation changes.")
    print()
    print("  ".join(value.ljust(widths[index]) for index, value in enumerate(headers)))
    print("  ".join("-" * width for width in widths))
    for row in table:
        print("  ".join(value.ljust(widths[index]) for index, value in enumerate(row)))
    print(f"\nCSV written to: {output_path}")


def main() -> int:
    args = parse_args()
    try:
        if not math.isfinite(args.timeout_seconds) or args.timeout_seconds <= 0.0:
            fail("--timeout-seconds must be finite and positive")
        for label, path in (
            ("solve executable", args.solve),
            ("scenario", args.scenario),
            ("prices", args.prices),
            ("inflows", args.inflows),
        ):
            if not path.is_file():
                fail(f"{label} does not exist: {path}")

        resolutions = [
            parse_resolution(value)
            for value in (args.resolution or DEFAULT_RESOLUTIONS)
        ]
        validate_resolutions(resolutions)
        rows, params = read_scenario(args.scenario)
        args.output_dir.mkdir(parents=True, exist_ok=True)
        results = [run_case(args, rows, params, resolution) for resolution in resolutions]
        add_finest_deltas(results)
        output_path = args.summary_output or args.output_dir / "resolution_analysis.csv"
        write_csv(results, output_path)
        print_table(results, output_path)
    except (OSError, ValueError, subprocess.SubprocessError, AnalysisError) as error:
        print(f"Resolution analysis failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
