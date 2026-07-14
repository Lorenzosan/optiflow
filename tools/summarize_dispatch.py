#!/usr/bin/env python3
"""Summarize pumped-storage dispatch economics and reservoir movement."""

from __future__ import annotations

import argparse
import csv
import math
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

from hydraulic_units import HYDRAULIC_POWER_FACTOR_MW_PER_FLOW_UNIT


class SummaryError(RuntimeError):
    pass


def fail(message: str) -> None:
    raise SummaryError(message)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Summarize an OptiFlow dispatch CSV.")
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--prices", required=True, type=Path)
    parser.add_argument("--inflows", required=True, type=Path)
    parser.add_argument("--dispatch", required=True, type=Path)
    return parser.parse_args()


def read_key_value_csv(path: Path) -> dict[str, str]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["key", "value"]:
            fail(f"{path}: expected header key,value")
        return {row["key"].strip(): row["value"].strip() for row in reader}


def numeric_param(params: dict[str, str], key: str) -> float:
    try:
        value = float(params[key])
    except (KeyError, ValueError) as error:
        raise SummaryError(f"scenario key {key} is missing or invalid") from error
    if not math.isfinite(value):
        fail(f"scenario key {key} is not finite")
    return value


def parse_timestamp_utc(value: str, context: str) -> datetime:
    if re.fullmatch(r"\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z", value) is None:
        fail(f"{context}: timestamp_utc must use YYYY-MM-DDTHH:MM:SSZ")
    try:
        return datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=timezone.utc)
    except ValueError as error:
        raise SummaryError(f"{context}: invalid timestamp_utc {value}") from error


def read_series(path: Path, value_column: str) -> tuple[list[str], list[datetime], list[float]]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle)
        if reader.fieldnames != ["timestamp_utc", value_column]:
            fail(f"{path}: expected header timestamp_utc,{value_column}")
        timestamp_texts: list[str] = []
        timestamps: list[datetime] = []
        values: list[float] = []
        for row_number, row in enumerate(reader, start=2):
            timestamp_text = row["timestamp_utc"].strip()
            timestamp = parse_timestamp_utc(timestamp_text, f"{path}:{row_number}")
            if timestamps and timestamp <= timestamps[-1]:
                fail(f"{path}:{row_number}: timestamps must be strictly increasing")
            try:
                value = float(row[value_column])
            except ValueError as error:
                raise SummaryError(f"{path}:{row_number}: invalid {value_column}") from error
            if not math.isfinite(value):
                fail(f"{path}:{row_number}: nonfinite {value_column}")
            timestamp_texts.append(timestamp_text)
            timestamps.append(timestamp)
            values.append(value)
        return timestamp_texts, timestamps, values


def read_dispatch(path: Path) -> list[dict[str, float | str]]:
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
        rows: list[dict[str, float | str]] = []
        for expected_index, row in enumerate(reader):
            if int(row["time_index"]) != expected_index:
                fail(f"{path}: invalid time_index at row {expected_index + 2}")
            timestamp_text = row["timestamp_utc"].strip()
            parse_timestamp_utc(timestamp_text, f"{path}:{expected_index + 2}")
            parsed: dict[str, float | str] = {
                key: float(value)
                for key, value in row.items()
                if key not in {"time_index", "timestamp_utc"}
            }
            parsed["time_index"] = float(expected_index)
            parsed["timestamp_utc"] = timestamp_text
            rows.append(parsed)
        return rows


def weighted_average(total: float, energy: float) -> str:
    return "n/a" if energy <= 0.0 else f"{total / energy:.6g}"


def metric(label: str, value: float | int | str, unit: str = "") -> None:
    suffix = f" {unit}" if unit else ""
    if isinstance(value, float):
        text = f"{0.0 if math.isclose(value, 0.0, abs_tol=5e-10) else value:.6g}"
    else:
        text = str(value)
    print(f"{label}: {text}{suffix}")


def summarize(args: argparse.Namespace) -> None:
    params = read_key_value_csv(args.scenario)
    price_timestamp_texts, price_timestamps, prices = read_series(args.prices, "price")
    inflow_timestamp_texts, inflow_timestamps, inflows = read_series(args.inflows, "natural_inflow")
    rows = read_dispatch(args.dispatch)
    if not rows:
        fail("dispatch file is empty")
    if len(rows) != len(prices) or len(prices) != len(inflows):
        fail("dispatch and input horizons differ")

    dt = numeric_param(params, "time_step_hours")
    interval_seconds = dt * 3600.0
    rounded_interval_seconds = round(interval_seconds)
    if not math.isclose(interval_seconds, rounded_interval_seconds, abs_tol=1.0e-9):
        fail("time_step_hours must resolve to a whole number of seconds")
    for index in range(len(rows)):
        if price_timestamp_texts[index] != inflow_timestamp_texts[index]:
            fail(f"price and inflow timestamps differ at index {index}")
        if rows[index]["timestamp_utc"] != price_timestamp_texts[index]:
            fail(f"dispatch timestamp mismatch at index {index}")
        if index > 0:
            actual_seconds = (price_timestamps[index] - price_timestamps[index - 1]).total_seconds()
            if actual_seconds != rounded_interval_seconds:
                fail(f"input timestamp spacing mismatch at index {index}")

    turbine_efficiency = numeric_param(params, "turbine_efficiency")
    pump_efficiency = numeric_param(params, "pump_efficiency")
    operating_cost_rate = numeric_param(params, "operating_cost_per_mwh")
    terminal_target = numeric_param(params, "terminal_target_reservoir_volume")

    export_mwh = import_mwh = 0.0
    export_revenue = import_cost = operating_cost = reward_total = 0.0
    turbine_generation = pump_consumption = 0.0
    turbine_price_sum = pump_price_sum = 0.0
    turbine_steps = pump_steps = spill_steps = wait_steps = 0

    for index, row in enumerate(rows):
        if not math.isclose(row["price"], prices[index], abs_tol=1e-9):
            fail(f"dispatch price mismatch at index {index}")
        if not math.isclose(row["natural_inflow"], inflows[index], abs_tol=1e-9):
            fail(f"dispatch inflow mismatch at index {index}")

        turbine_power = (
            row["turbine_flow"]
            * HYDRAULIC_POWER_FACTOR_MW_PER_FLOW_UNIT
            * turbine_efficiency
        )
        pump_power = (
            row["pump_flow"]
            * HYDRAULIC_POWER_FACTOR_MW_PER_FLOW_UNIT
            / pump_efficiency
        )
        exported = max(row["net_power"], 0.0) * dt
        imported = max(-row["net_power"], 0.0) * dt
        turbine_energy = turbine_power * dt
        pump_energy = pump_power * dt

        export_mwh += exported
        import_mwh += imported
        export_revenue += row["price"] * exported
        import_cost += row["price"] * imported
        turbine_generation += turbine_energy
        pump_consumption += pump_energy
        operating_cost += operating_cost_rate * (turbine_power + pump_power) * dt
        reward_total += row["reward"]
        turbine_price_sum += row["price"] * turbine_energy
        pump_price_sum += row["price"] * pump_energy

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
        if not active:
            wait_steps += 1

    net_market_cashflow = export_revenue - import_cost
    recomputed_reward = net_market_cashflow - operating_cost
    reported_profit = rows[-1]["cumulative_profit"]

    print("OptiFlow dispatch summary")
    metric("Scenario", params.get("scenario_name", ""))
    metric("Rows", len(rows))
    metric("Time step hours", dt)
    print()
    metric("Export revenue", export_revenue, "€")
    metric("Import cost", import_cost, "€")
    metric("Net market cashflow", net_market_cashflow, "€")
    metric("Operating cost", operating_cost, "€")
    metric("Recomputed reward", recomputed_reward, "€")
    metric("Dispatch reward sum", reward_total, "€")
    metric("Reported cumulative profit", reported_profit, "€")
    metric("Reward difference", reported_profit - reward_total, "€")
    print()
    metric("Export energy", export_mwh, "MWh")
    metric("Import energy", import_mwh, "MWh")
    metric("Net export energy", export_mwh - import_mwh, "MWh")
    metric("Turbine generation", turbine_generation, "MWh")
    metric("Pump consumption", pump_consumption, "MWh")
    print()
    metric("Average export price", weighted_average(export_revenue, export_mwh), "€/MWh")
    metric("Average import price", weighted_average(import_cost, import_mwh), "€/MWh")
    metric("Average turbine price", weighted_average(turbine_price_sum, turbine_generation), "€/MWh")
    metric("Average pump price", weighted_average(pump_price_sum, pump_consumption), "€/MWh")
    print()
    metric("Turbine steps", turbine_steps)
    metric("Pump steps", pump_steps)
    metric("Spill steps", spill_steps)
    metric("Wait steps", wait_steps)
    print()
    metric("Initial reservoir volume", rows[0]["reservoir_volume"], "10³ m³")
    metric("Final reservoir volume", rows[-1]["next_reservoir_volume"], "10³ m³")
    metric("Terminal reservoir target", terminal_target, "10³ m³")
    metric("Terminal reservoir target deviation", rows[-1]["next_reservoir_volume"] - terminal_target, "10³ m³")


def main() -> int:
    try:
        summarize(parse_args())
    except (OSError, ValueError, SummaryError) as error:
        print(f"Dispatch summary failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
