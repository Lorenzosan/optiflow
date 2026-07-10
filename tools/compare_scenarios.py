#!/usr/bin/env python3
"""Run and compare pumped-storage scenarios on common exogenous inputs."""

from __future__ import annotations

import argparse
import csv
import math
import re
import subprocess
import sys
from pathlib import Path


class ComparisonError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise ComparisonError(message)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare OptiFlow scenario CSV files.")
    parser.add_argument("--solve", required=True, type=Path)
    parser.add_argument("--prices", required=True, type=Path)
    parser.add_argument("--inflows", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--scenario", required=True, action="append", type=Path)
    parser.add_argument("--summary-output", type=Path)
    return parser.parse_args()


def read_params(path: Path) -> dict[str, str]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["key", "value"]:
            fail(f"{path}: expected header key,value")
        return {row["key"].strip(): row["value"].strip() for row in reader}


def read_dispatch(path: Path) -> list[dict[str, float]]:
    expected = [
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
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != expected:
            fail(f"{path}: unexpected dispatch header")
        rows: list[dict[str, float]] = []
        for row in reader:
            rows.append({
                key: float(value)
                for key, value in row.items()
                if key != "timestamp_utc"
            })
        return rows


def safe_name(name: str) -> str:
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", name.strip()) or "scenario"


def parse_stdout(text: str) -> dict[str, str]:
    mapping = {
        "Time steps": "time_steps",
        "Reservoir grid points": "reservoir_grid_points",
        "Action count": "action_count",
        "Solve seconds": "solve_seconds",
        "Simulation seconds": "simulation_seconds",
    }
    result: dict[str, str] = {}
    for line in text.splitlines():
        if ":" in line:
            label, value = line.split(":", 1)
            if label.strip() in mapping:
                result[mapping[label.strip()]] = value.strip()
    return result


def number(value: float) -> str:
    return f"{0.0 if math.isclose(value, 0.0, abs_tol=5e-10) else value:.10g}"


def run_scenario(args: argparse.Namespace, scenario_path: Path) -> dict[str, str]:
    params = read_params(scenario_path)
    name = params.get("scenario_name", scenario_path.stem)
    stem = safe_name(name)
    dispatch_path = args.output_dir / f"{stem}_dispatch.csv"
    stdout_path = args.output_dir / f"{stem}_stdout.txt"
    completed = subprocess.run(
        [
            str(args.solve),
            "--scenario", str(scenario_path),
            "--prices", str(args.prices),
            "--inflows", str(args.inflows),
            "--output", str(dispatch_path),
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    stdout_path.write_text(completed.stdout)
    if completed.returncode != 0:
        fail(f"scenario {scenario_path} failed: {completed.stderr.strip()}")

    rows = read_dispatch(dispatch_path)
    if not rows:
        fail(f"scenario {scenario_path} produced an empty dispatch")
    dt = float(params["time_step_hours"])
    export_mwh = sum(max(row["net_power"], 0.0) * dt for row in rows)
    import_mwh = sum(max(-row["net_power"], 0.0) * dt for row in rows)
    turbine_steps = sum(row["turbine_flow"] > 0.0 for row in rows)
    pump_steps = sum(row["pump_flow"] > 0.0 for row in rows)
    spill_steps = sum(row["spill_flow"] > 0.0 for row in rows)
    wait_steps = sum(
        row["turbine_flow"] == 0.0 and row["pump_flow"] == 0.0 and row["spill_flow"] == 0.0
        for row in rows
    )
    result = {
        "scenario": name,
        "rows": str(len(rows)),
        "cumulative_profit": number(rows[-1]["cumulative_profit"]),
        "export_mwh": number(export_mwh),
        "import_mwh": number(import_mwh),
        "net_export_mwh": number(export_mwh - import_mwh),
        "initial_reservoir": number(rows[0]["reservoir_volume"]),
        "final_reservoir": number(rows[-1]["next_reservoir_volume"]),
        "turbine_steps": str(turbine_steps),
        "pump_steps": str(pump_steps),
        "spill_steps": str(spill_steps),
        "wait_steps": str(wait_steps),
        "dispatch_file": str(dispatch_path),
        "stdout_file": str(stdout_path),
    }
    result.update(parse_stdout(completed.stdout))
    return result


FIELDNAMES = [
    "scenario",
    "rows",
    "cumulative_profit",
    "export_mwh",
    "import_mwh",
    "net_export_mwh",
    "initial_reservoir",
    "final_reservoir",
    "turbine_steps",
    "pump_steps",
    "spill_steps",
    "wait_steps",
    "time_steps",
    "reservoir_grid_points",
    "action_count",
    "solve_seconds",
    "simulation_seconds",
    "dispatch_file",
    "stdout_file",
]


def write_results(rows: list[dict[str, str]], output: Path | None) -> None:
    handle = output.open("w", newline="") if output else sys.stdout
    try:
        writer = csv.DictWriter(handle, fieldnames=FIELDNAMES, extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)
    finally:
        if output:
            handle.close()


def print_table(rows: list[dict[str, str]], output: Path | None) -> None:
    base = float(rows[0]["cumulative_profit"])
    headers = ["Scenario", "Profit", "Delta", "Export MWh", "Import MWh", "Final res.", "Actions", "Solve s"]
    rendered = []
    for row in rows:
        profit = float(row["cumulative_profit"])
        rendered.append([
            row["scenario"],
            f"{profit:.0f}",
            f"{profit - base:.0f}",
            f"{float(row['export_mwh']):.1f}",
            f"{float(row['import_mwh']):.1f}",
            f"{float(row['final_reservoir']):.2f}",
            row.get("action_count", ""),
            f"{float(row.get('solve_seconds', '0')):.3f}",
        ])
    widths = [max(len(header), *(len(row[index]) for row in rendered)) for index, header in enumerate(headers)]
    print("Scenario comparison")
    print("Delta column is measured relative to the first scenario.")
    print()
    print("  ".join(value.ljust(widths[index]) if index == 0 else value.rjust(widths[index]) for index, value in enumerate(headers)))
    print("  ".join("-" * width for width in widths))
    for row in rendered:
        print("  ".join(value.ljust(widths[index]) if index == 0 else value.rjust(widths[index]) for index, value in enumerate(row)))
    if output:
        print(f"\nCSV written to: {output}")


def main() -> int:
    args = parse_args()
    try:
        if not args.solve.exists():
            fail(f"solve executable does not exist: {args.solve}")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        rows = [run_scenario(args, scenario) for scenario in args.scenario]
        write_results(rows, args.summary_output)
        if args.summary_output:
            print_table(rows, args.summary_output)
    except (OSError, ValueError, subprocess.SubprocessError, ComparisonError) as error:
        print(f"Scenario comparison failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
