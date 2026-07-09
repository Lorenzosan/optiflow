#!/usr/bin/env python3
"""Validate an OptiFlow dispatch CSV against its input scenario and exogenous series."""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from pathlib import Path


DEFAULT_STATE_TOLERANCE = 1.0e-4
DEFAULT_ECONOMIC_TOLERANCE = 1.0e-2
DEFAULT_CUMULATIVE_RELATIVE_TOLERANCE = 2.0e-5


class ValidationError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise ValidationError(message)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate an OptiFlow dispatch CSV.")
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--prices", required=True, type=Path)
    parser.add_argument("--inflows", required=True, type=Path)
    parser.add_argument("--dispatch", required=True, type=Path)
    parser.add_argument("--stdout", type=Path, help="Optional CLI stdout capture to validate diagnostics.")
    parser.add_argument("--state-tolerance", type=float, default=DEFAULT_STATE_TOLERANCE)
    parser.add_argument("--economic-tolerance", type=float, default=DEFAULT_ECONOMIC_TOLERANCE)
    parser.add_argument("--cumulative-relative-tolerance", type=float,
                        default=DEFAULT_CUMULATIVE_RELATIVE_TOLERANCE)
    return parser.parse_args()


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
        raise ValidationError(f"scenario key {key} is not numeric: {params[key]}") from error
    if not math.isfinite(value):
        fail(f"scenario key {key} is not finite")
    return value


def integer_param(params: dict[str, str], key: str) -> int:
    value = numeric_param(params, key)
    if not value.is_integer() or value < 0.0:
        fail(f"scenario key {key} must be a nonnegative integer")
    return int(value)


def read_series(path: Path, value_column: str) -> list[float]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["time_index", value_column]:
            fail(f"{path}: expected header time_index,{value_column}")
        values: list[float] = []
        for expected_index, row in enumerate(reader):
            try:
                time_index = int(row["time_index"])
                value = float(row[value_column])
            except ValueError as error:
                raise ValidationError(f"{path}: invalid row at index {expected_index}") from error
            if time_index != expected_index:
                fail(f"{path}: expected time_index {expected_index}, got {time_index}")
            if not math.isfinite(value):
                fail(f"{path}: nonfinite {value_column} at index {expected_index}")
            if value_column == "natural_inflow" and value < 0.0:
                fail(f"{path}: negative natural_inflow at index {expected_index}")
            values.append(value)
        return values


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
                raise ValidationError(f"{path}: invalid numeric value at row {expected_index + 2}") from error
            if time_index != expected_index:
                fail(f"{path}: expected time_index {expected_index}, got {time_index}")
            for key, value in parsed.items():
                if not math.isfinite(value):
                    fail(f"{path}: nonfinite {key} at index {expected_index}")
            parsed["time_index"] = float(time_index)
            rows.append(parsed)
        return rows


def is_close(actual: float, expected: float, absolute_tolerance: float, relative_tolerance: float = 0.0) -> bool:
    limit = max(absolute_tolerance, relative_tolerance * max(abs(actual), abs(expected), 1.0))
    return abs(actual - expected) <= limit


def require_close(label: str,
                  index: int,
                  actual: float,
                  expected: float,
                  absolute_tolerance: float,
                  relative_tolerance: float = 0.0) -> None:
    if not is_close(actual, expected, absolute_tolerance, relative_tolerance):
        fail(f"{label} mismatch at index {index}: expected {expected}, got {actual}")


def axis_values(max_value: float, steps: int) -> list[float]:
    if steps <= 0:
        fail("action step count must be positive")
    if steps == 1:
        return [0.0]
    return [max_value * index / float(steps - 1) for index in range(steps)]


def require_on_axis(label: str, index: int, value: float, axis: list[float], tolerance: float) -> None:
    if not any(is_close(value, candidate, tolerance) for candidate in axis):
        fail(f"{label} at index {index} is not on the configured action axis: {value}")


def validate_stdout(stdout_path: Path,
                    dispatch_rows: list[dict[str, float]],
                    scenario_name: str,
                    reservoir_grid_points: int,
                    battery_grid_points: int,
                    action_count: int) -> None:
    text = stdout_path.read_text()

    expected_strings = [
        f"Scenario: {scenario_name}",
        f"Time steps: {len(dispatch_rows)}",
        f"Reservoir grid points: {reservoir_grid_points}",
        f"Battery grid points: {battery_grid_points}",
        f"Action count: {action_count}",
    ]
    for expected in expected_strings:
        if expected not in text:
            fail(f"stdout does not contain expected diagnostic line: {expected}")

    counters = {
        "Turbine steps": sum(1 for row in dispatch_rows if row["turbine_flow"] > 0.0),
        "Pump steps": sum(1 for row in dispatch_rows if row["pump_flow"] > 0.0),
        "Spill steps": sum(1 for row in dispatch_rows if row["spill_flow"] > 0.0),
        "Battery charge steps": sum(1 for row in dispatch_rows if row["battery_charge_power"] > 0.0),
        "Battery discharge steps": sum(1 for row in dispatch_rows if row["battery_discharge_power"] > 0.0),
        "Wait steps": sum(
            1 for row in dispatch_rows
            if row["turbine_flow"] == 0.0
            and row["pump_flow"] == 0.0
            and row["spill_flow"] == 0.0
            and row["battery_charge_power"] == 0.0
            and row["battery_discharge_power"] == 0.0
        ),
    }
    for label, expected_value in counters.items():
        pattern = rf"^{re.escape(label)}: {expected_value}$"
        if re.search(pattern, text, flags=re.MULTILINE) is None:
            fail(f"stdout does not contain expected diagnostic line: {label}: {expected_value}")


def validate(args: argparse.Namespace) -> None:
    params = read_key_value_csv(args.scenario)
    prices = read_series(args.prices, "price")
    inflows = read_series(args.inflows, "natural_inflow")
    dispatch_rows = read_dispatch(args.dispatch)

    if len(prices) != len(inflows):
        fail(f"price and inflow row counts differ: {len(prices)} vs {len(inflows)}")
    if len(dispatch_rows) != len(prices):
        fail(f"dispatch row count {len(dispatch_rows)} does not match input horizon {len(prices)}")

    dt = numeric_param(params, "time_step_hours")
    reservoir_min = numeric_param(params, "reservoir_min_volume")
    reservoir_max = numeric_param(params, "reservoir_max_volume")
    battery_min = numeric_param(params, "battery_min_soc")
    battery_max = numeric_param(params, "battery_max_soc")
    initial_reservoir = numeric_param(params, "initial_reservoir_volume")
    initial_battery = numeric_param(params, "initial_battery_soc")
    terminal_reservoir_min = numeric_param(params, "terminal_reservoir_min_volume")
    terminal_reservoir_max = numeric_param(params, "terminal_reservoir_max_volume")
    terminal_battery_min = numeric_param(params, "terminal_battery_min_soc")
    terminal_battery_max = numeric_param(params, "terminal_battery_max_soc")
    turbine_max_flow = numeric_param(params, "turbine_max_flow")
    pump_max_flow = numeric_param(params, "pump_max_flow")
    spill_max_flow = numeric_param(params, "spill_max_flow")
    battery_max_charge_power = numeric_param(params, "battery_max_charge_power")
    battery_max_discharge_power = numeric_param(params, "battery_max_discharge_power")
    turbine_efficiency = numeric_param(params, "turbine_efficiency")
    pump_efficiency = numeric_param(params, "pump_efficiency")
    battery_charge_efficiency = numeric_param(params, "battery_charge_efficiency")
    battery_discharge_efficiency = numeric_param(params, "battery_discharge_efficiency")
    water_to_power_factor = numeric_param(params, "water_to_power_factor")
    degradation_cost = numeric_param(params, "battery_degradation_cost_per_mwh")
    operating_cost = numeric_param(params, "operating_cost_per_mwh")

    reservoir_grid_points = integer_param(params, "reservoir_volume_grid_points")
    battery_grid_points = integer_param(params, "battery_soc_grid_points")
    turbine_flow_steps = integer_param(params, "turbine_flow_steps")
    spill_flow_steps = integer_param(params, "spill_flow_steps")
    pump_flow_steps = integer_param(params, "pump_flow_steps")
    battery_charge_steps = integer_param(params, "battery_charge_steps")
    battery_discharge_steps = integer_param(params, "battery_discharge_steps")
    action_count = (turbine_flow_steps * spill_flow_steps * pump_flow_steps *
                    battery_charge_steps * battery_discharge_steps)

    action_axes = {
        "turbine_flow": axis_values(turbine_max_flow, turbine_flow_steps),
        "spill_flow": axis_values(spill_max_flow, spill_flow_steps),
        "pump_flow": axis_values(pump_max_flow, pump_flow_steps),
        "battery_charge_power": axis_values(battery_max_charge_power, battery_charge_steps),
        "battery_discharge_power": axis_values(battery_max_discharge_power, battery_discharge_steps),
    }

    cumulative_profit = 0.0
    previous_next_reservoir = initial_reservoir
    previous_next_battery = initial_battery

    for index, row in enumerate(dispatch_rows):
        require_close("price", index, row["price"], prices[index], args.state_tolerance)
        require_close("natural_inflow", index, row["natural_inflow"], inflows[index], args.state_tolerance)
        require_close("reservoir continuity", index, row["reservoir_volume"], previous_next_reservoir,
                      args.state_tolerance)
        require_close("battery continuity", index, row["battery_soc"], previous_next_battery,
                      args.state_tolerance)

        if row["reservoir_volume"] < reservoir_min - args.state_tolerance:
            fail(f"reservoir below lower bound at index {index}")
        if row["reservoir_volume"] > reservoir_max + args.state_tolerance:
            fail(f"reservoir above upper bound at index {index}")
        if row["battery_soc"] < battery_min - args.state_tolerance:
            fail(f"battery below lower bound at index {index}")
        if row["battery_soc"] > battery_max + args.state_tolerance:
            fail(f"battery above upper bound at index {index}")

        for label, axis in action_axes.items():
            if row[label] < -args.state_tolerance:
                fail(f"{label} is negative at index {index}")
            require_on_axis(label, index, row[label], axis, args.state_tolerance)

        if row["turbine_flow"] > args.state_tolerance and row["pump_flow"] > args.state_tolerance:
            fail(f"simultaneous turbine and pump at index {index}")
        if (row["battery_charge_power"] > args.state_tolerance and
                row["battery_discharge_power"] > args.state_tolerance):
            fail(f"simultaneous battery charge and discharge at index {index}")

        expected_next_reservoir = (row["reservoir_volume"] + row["natural_inflow"] * dt +
                                   row["pump_flow"] * dt - row["turbine_flow"] * dt -
                                   row["spill_flow"] * dt)
        expected_next_battery = (row["battery_soc"] +
                                 row["battery_charge_power"] * battery_charge_efficiency * dt -
                                 row["battery_discharge_power"] / battery_discharge_efficiency * dt)
        require_close("next_reservoir_volume", index, row["next_reservoir_volume"], expected_next_reservoir,
                      args.state_tolerance)
        require_close("next_battery_soc", index, row["next_battery_soc"], expected_next_battery,
                      args.state_tolerance)

        if row["next_reservoir_volume"] < reservoir_min - args.state_tolerance:
            fail(f"next reservoir below lower bound at index {index}")
        if row["next_reservoir_volume"] > reservoir_max + args.state_tolerance:
            fail(f"next reservoir above upper bound at index {index}")
        if row["next_battery_soc"] < battery_min - args.state_tolerance:
            fail(f"next battery below lower bound at index {index}")
        if row["next_battery_soc"] > battery_max + args.state_tolerance:
            fail(f"next battery above upper bound at index {index}")

        turbine_power = row["turbine_flow"] * water_to_power_factor * turbine_efficiency
        pump_power = row["pump_flow"] * water_to_power_factor / pump_efficiency
        expected_net_power = (turbine_power + row["battery_discharge_power"] - pump_power -
                              row["battery_charge_power"])
        operating_throughput = (turbine_power + pump_power + row["battery_charge_power"] +
                                row["battery_discharge_power"]) * dt
        battery_throughput = (row["battery_charge_power"] + row["battery_discharge_power"]) * dt
        expected_reward = (row["price"] * expected_net_power * dt - operating_cost * operating_throughput -
                           degradation_cost * battery_throughput)
        cumulative_profit += row["reward"]

        require_close("net_power", index, row["net_power"], expected_net_power, args.state_tolerance)
        require_close("reward", index, row["reward"], expected_reward, args.economic_tolerance)
        require_close("cumulative_profit", index, row["cumulative_profit"], cumulative_profit,
                      args.economic_tolerance, args.cumulative_relative_tolerance)

        cumulative_profit = row["cumulative_profit"]
        previous_next_reservoir = row["next_reservoir_volume"]
        previous_next_battery = row["next_battery_soc"]

    final = dispatch_rows[-1]
    if final["next_reservoir_volume"] < terminal_reservoir_min - args.state_tolerance:
        fail("final reservoir is below terminal lower bound")
    if final["next_reservoir_volume"] > terminal_reservoir_max + args.state_tolerance:
        fail("final reservoir is above terminal upper bound")
    if final["next_battery_soc"] < terminal_battery_min - args.state_tolerance:
        fail("final battery is below terminal lower bound")
    if final["next_battery_soc"] > terminal_battery_max + args.state_tolerance:
        fail("final battery is above terminal upper bound")

    if args.stdout is not None:
        scenario_name = params.get("scenario_name", "")
        validate_stdout(args.stdout,
                        dispatch_rows,
                        scenario_name,
                        reservoir_grid_points,
                        battery_grid_points,
                        action_count)

    print(f"Dispatch validation passed: {len(dispatch_rows)} rows, final cumulative profit "
          f"{dispatch_rows[-1]['cumulative_profit']:.6g}")


def main() -> int:
    try:
        validate(parse_args())
    except ValidationError as error:
        print(f"Dispatch validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
