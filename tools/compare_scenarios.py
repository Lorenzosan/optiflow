#!/usr/bin/env python3
"""Run and compare multiple OptiFlow CSV scenarios on common exogenous inputs."""

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


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compare OptiFlow scenario CSV files.")
    parser.add_argument("--solve", required=True, type=Path, help="Path to the optiflow_solve executable.")
    parser.add_argument("--prices", required=True, type=Path, help="Common prices CSV.")
    parser.add_argument("--inflows", required=True, type=Path, help="Common inflows CSV.")
    parser.add_argument("--output-dir", required=True, type=Path, help="Directory for generated dispatch/stdout files.")
    parser.add_argument(
        "--scenario",
        required=True,
        action="append",
        type=Path,
        help="Scenario CSV to run. May be specified multiple times.",
    )
    parser.add_argument(
        "--summary-output",
        type=Path,
        help="Optional CSV file for the comparison table. Defaults to stdout.",
    )
    return parser.parse_args()


def fail(message: str) -> None:
    raise ComparisonError(message)


def read_key_value_csv(path: Path) -> dict[str, str]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["key", "value"]:
            fail(f"{path}: expected header key,value")
        values: dict[str, str] = {}
        for row_number, row in enumerate(reader, start=2):
            key = row.get("key", "").strip()
            value = row.get("value", "").strip()
            if not key:
                fail(f"{path}:{row_number}: empty key")
            if key in values:
                fail(f"{path}:{row_number}: duplicate key {key}")
            values[key] = value
        return values


def numeric_param(params: dict[str, str], key: str) -> float:
    if key not in params:
        fail(f"scenario is missing required key {key}")
    try:
        value = float(params[key])
    except ValueError as error:
        raise ComparisonError(f"scenario key {key} is not numeric: {params[key]}") from error
    if not math.isfinite(value):
        fail(f"scenario key {key} is not finite")
    return value


def read_dispatch(path: Path) -> list[dict[str, float]]:
    expected_header = [
        "time_index",
        "price",
        "natural_inflow",
        "reservoir_volume",
        "battery_soc",
        "turbine_flow",
        "spill_flow",
        "pump_flow",
        "battery_charge_power",
        "battery_discharge_power",
        "next_reservoir_volume",
        "next_battery_soc",
        "net_power",
        "reward",
        "cumulative_profit",
    ]
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != expected_header:
            fail(f"{path}: unexpected dispatch header")
        rows: list[dict[str, float]] = []
        for expected_index, row in enumerate(reader):
            try:
                time_index = int(row["time_index"])
                parsed = {key: float(value) for key, value in row.items() if key != "time_index"}
            except ValueError as error:
                raise ComparisonError(f"{path}: invalid numeric value at row {expected_index + 2}") from error
            if time_index != expected_index:
                fail(f"{path}: expected time_index {expected_index}, got {time_index}")
            for key, value in parsed.items():
                if not math.isfinite(value):
                    fail(f"{path}: nonfinite {key} at index {expected_index}")
            parsed["time_index"] = float(time_index)
            rows.append(parsed)
        return rows


def parse_stdout_metrics(text: str) -> dict[str, str]:
    metrics: dict[str, str] = {}
    label_to_key = {
        "Time steps": "time_steps",
        "Reservoir grid points": "reservoir_grid_points",
        "Battery grid points": "battery_grid_points",
        "Action count": "action_count",
        "Solve seconds": "solve_seconds",
        "Simulation seconds": "simulation_seconds",
    }
    for raw_line in text.splitlines():
        if ":" not in raw_line:
            continue
        label, value = raw_line.split(":", 1)
        label = label.strip()
        if label in label_to_key:
            metrics[label_to_key[label]] = value.strip()
    return metrics


def safe_name(name: str) -> str:
    cleaned = re.sub(r"[^A-Za-z0-9_.-]+", "_", name.strip())
    return cleaned or "scenario"


def format_number(value: float) -> str:
    if math.isclose(value, 0.0, abs_tol=5.0e-10):
        value = 0.0
    return f"{value:.10g}"


def run_scenario(args: argparse.Namespace, scenario_path: Path) -> dict[str, str]:
    params = read_key_value_csv(scenario_path)
    scenario_name = params.get("scenario_name", scenario_path.stem)
    output_stem = safe_name(scenario_name)
    dispatch_path = args.output_dir / f"{output_stem}_dispatch.csv"
    stdout_path = args.output_dir / f"{output_stem}_stdout.txt"

    command = [
        str(args.solve),
        "--scenario",
        str(scenario_path),
        "--prices",
        str(args.prices),
        "--inflows",
        str(args.inflows),
        "--output",
        str(dispatch_path),
    ]
    completed = subprocess.run(command, text=True, capture_output=True, check=False)
    stdout_path.write_text(completed.stdout)
    if completed.stderr:
        (args.output_dir / f"{output_stem}_stderr.txt").write_text(completed.stderr)
    if completed.returncode != 0:
        fail(f"scenario {scenario_path} failed with exit code {completed.returncode}: {completed.stderr.strip()}")

    rows = read_dispatch(dispatch_path)
    if not rows:
        fail(f"scenario {scenario_path} produced an empty dispatch")

    dt = numeric_param(params, "time_step_hours")
    stdout_metrics = parse_stdout_metrics(completed.stdout)

    export_mwh = 0.0
    import_mwh = 0.0
    turbine_steps = 0
    pump_steps = 0
    spill_steps = 0
    battery_charge_steps = 0
    battery_discharge_steps = 0
    wait_steps = 0

    for row in rows:
        net_power = row["net_power"]
        export_mwh += max(net_power, 0.0) * dt
        import_mwh += max(-net_power, 0.0) * dt
        active = False
        if row["turbine_flow"] > 0.0:
            turbine_steps += 1
            active = True
        if row["pump_flow"] > 0.0:
            pump_steps += 1
            active = True
        if row["spill_flow"] > 0.0:
            spill_steps += 1
            active = True
        if row["battery_charge_power"] > 0.0:
            battery_charge_steps += 1
            active = True
        if row["battery_discharge_power"] > 0.0:
            battery_discharge_steps += 1
            active = True
        if not active:
            wait_steps += 1

    first = rows[0]
    last = rows[-1]
    result = {
        "scenario": scenario_name,
        "rows": str(len(rows)),
        "cumulative_profit": format_number(last["cumulative_profit"]),
        "export_mwh": format_number(export_mwh),
        "import_mwh": format_number(import_mwh),
        "net_export_mwh": format_number(export_mwh - import_mwh),
        "initial_reservoir": format_number(first["reservoir_volume"]),
        "final_reservoir": format_number(last["next_reservoir_volume"]),
        "initial_battery_soc": format_number(first["battery_soc"]),
        "final_battery_soc": format_number(last["next_battery_soc"]),
        "turbine_steps": str(turbine_steps),
        "pump_steps": str(pump_steps),
        "spill_steps": str(spill_steps),
        "battery_charge_steps": str(battery_charge_steps),
        "battery_discharge_steps": str(battery_discharge_steps),
        "wait_steps": str(wait_steps),
        "dispatch_file": str(dispatch_path),
        "stdout_file": str(stdout_path),
    }
    result.update(stdout_metrics)
    return result


def write_results(rows: list[dict[str, str]], output: Path | None) -> None:
    fieldnames = [
        "scenario",
        "rows",
        "cumulative_profit",
        "export_mwh",
        "import_mwh",
        "net_export_mwh",
        "initial_reservoir",
        "final_reservoir",
        "initial_battery_soc",
        "final_battery_soc",
        "turbine_steps",
        "pump_steps",
        "spill_steps",
        "battery_charge_steps",
        "battery_discharge_steps",
        "wait_steps",
        "time_steps",
        "reservoir_grid_points",
        "battery_grid_points",
        "action_count",
        "solve_seconds",
        "simulation_seconds",
        "dispatch_file",
        "stdout_file",
    ]
    handle = output.open("w", newline="") if output else sys.stdout
    try:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, extrasaction="ignore")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)
    finally:
        if output:
            handle.close()


def parse_float_field(row: dict[str, str], key: str) -> float:
    try:
        return float(row[key])
    except (KeyError, ValueError) as error:
        raise ComparisonError(f"comparison row has invalid numeric field {key}") from error


def format_table_number(value: float, decimals: int = 2) -> str:
    if math.isclose(value, 0.0, abs_tol=5.0e-10):
        value = 0.0
    return f"{value:.{decimals}f}"


def print_human_summary(rows: list[dict[str, str]], output: Path | None) -> None:
    if not rows:
        return

    base = rows[0]
    base_profit = parse_float_field(base, "cumulative_profit")

    table_rows: list[list[str]] = []
    for row in rows:
        profit = parse_float_field(row, "cumulative_profit")
        delta = profit - base_profit
        export_mwh = parse_float_field(row, "export_mwh")
        import_mwh = parse_float_field(row, "import_mwh")
        net_export_mwh = parse_float_field(row, "net_export_mwh")
        final_reservoir = parse_float_field(row, "final_reservoir")
        final_battery_soc = parse_float_field(row, "final_battery_soc")
        solve_seconds = parse_float_field(row, "solve_seconds") if "solve_seconds" in row else 0.0

        table_rows.append([
            row.get("scenario", ""),
            format_table_number(profit, 0),
            format_table_number(delta, 0),
            format_table_number(export_mwh, 1),
            format_table_number(import_mwh, 1),
            format_table_number(net_export_mwh, 1),
            format_table_number(final_reservoir, 2),
            format_table_number(final_battery_soc, 2),
            row.get("action_count", ""),
            format_table_number(solve_seconds, 3),
        ])

    headers = [
        "Scenario",
        "Profit",
        "Delta",
        "Export MWh",
        "Import MWh",
        "Net MWh",
        "Final res.",
        "Final batt.",
        "Actions",
        "Solve s",
    ]
    widths = [len(header) for header in headers]
    for table_row in table_rows:
        widths = [max(width, len(cell)) for width, cell in zip(widths, table_row)]

    def render(values: list[str]) -> str:
        return "  ".join(value.rjust(width) if index > 0 else value.ljust(width)
                         for index, (value, width) in enumerate(zip(values, widths)))

    print("Scenario comparison")
    print(render(headers))
    print(render(["-" * width for width in widths]))
    for table_row in table_rows:
        print(render(table_row))

    if len(rows) >= 2:
        comparison_profit = parse_float_field(rows[1], "cumulative_profit")
        delta = comparison_profit - base_profit
        print()
        print(f"Delta vs {base.get('scenario', 'base')}: {format_table_number(delta, 0)}")

    if output is not None:
        print()
        print(f"CSV written to: {output}")


def main() -> int:
    args = parse_args()
    try:
        if not args.solve.exists():
            fail(f"solve executable does not exist: {args.solve}")
        args.output_dir.mkdir(parents=True, exist_ok=True)
        rows = [run_scenario(args, scenario) for scenario in args.scenario]
        write_results(rows, args.summary_output)
        if args.summary_output is not None:
            print_human_summary(rows, args.summary_output)
    except (OSError, subprocess.SubprocessError, ComparisonError) as error:
        print(f"Scenario comparison failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
