#!/usr/bin/env python3
"""Summarize OptiFlow dispatch economics and inventory movement."""

from __future__ import annotations

import argparse
import csv
import math
import sys
from pathlib import Path


class SummaryError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Summarize an OptiFlow dispatch CSV.")
    parser.add_argument("--scenario", required=True, type=Path)
    parser.add_argument("--prices", required=True, type=Path)
    parser.add_argument("--inflows", required=True, type=Path)
    parser.add_argument("--dispatch", required=True, type=Path)
    return parser.parse_args()


def fail(message: str) -> None:
    raise SummaryError(message)


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
        raise SummaryError(f"scenario key {key} is not numeric: {params[key]}") from error
    if not math.isfinite(value):
        fail(f"scenario key {key} is not finite")
    return value


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
                raise SummaryError(f"{path}: invalid row at index {expected_index}") from error
            if time_index != expected_index:
                fail(f"{path}: expected time_index {expected_index}, got {time_index}")
            if not math.isfinite(value):
                fail(f"{path}: nonfinite {value_column} at index {expected_index}")
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
                raise SummaryError(f"{path}: invalid numeric value at row {expected_index + 2}") from error
            if time_index != expected_index:
                fail(f"{path}: expected time_index {expected_index}, got {time_index}")
            for key, value in parsed.items():
                if not math.isfinite(value):
                    fail(f"{path}: nonfinite {key} at index {expected_index}")
            parsed["time_index"] = float(time_index)
            rows.append(parsed)
        return rows


def weighted_average_price(weighted_price_sum: float, energy: float) -> str:
    if energy <= 0.0:
        return "n/a"
    return format_number(weighted_price_sum / energy)


def format_number(value: float) -> str:
    if math.isclose(value, 0.0, abs_tol=5.0e-10):
        value = 0.0
    return f"{value:.6g}"


def print_metric(label: str, value: float | str, unit: str = "") -> None:
    suffix = f" {unit}" if unit else ""
    if isinstance(value, str):
        print(f"{label}: {value}{suffix}")
    else:
        print(f"{label}: {format_number(value)}{suffix}")


def summarize(args: argparse.Namespace) -> None:
    params = read_key_value_csv(args.scenario)
    prices = read_series(args.prices, "price")
    inflows = read_series(args.inflows, "natural_inflow")
    dispatch_rows = read_dispatch(args.dispatch)

    if len(prices) != len(inflows):
        fail(f"price and inflow row counts differ: {len(prices)} vs {len(inflows)}")
    if len(dispatch_rows) != len(prices):
        fail(f"dispatch row count {len(dispatch_rows)} does not match input horizon {len(prices)}")

    dt = numeric_param(params, "time_step_hours")
    water_to_power = numeric_param(params, "water_to_power_factor")
    turbine_efficiency = numeric_param(params, "turbine_efficiency")
    pump_efficiency = numeric_param(params, "pump_efficiency")
    operating_cost = numeric_param(params, "operating_cost_per_mwh")
    battery_degradation_cost = numeric_param(params, "battery_degradation_cost_per_mwh")
    terminal_reservoir_target = numeric_param(params, "terminal_target_reservoir_volume")
    terminal_battery_target = numeric_param(params, "terminal_target_battery_soc")

    export_mwh = 0.0
    import_mwh = 0.0
    export_revenue = 0.0
    import_cost = 0.0
    turbine_generation_mwh = 0.0
    pump_consumption_mwh = 0.0
    battery_charge_mwh = 0.0
    battery_discharge_mwh = 0.0
    operating_cost_total = 0.0
    battery_degradation_total = 0.0
    reward_total = 0.0

    turbine_price_sum = 0.0
    pump_price_sum = 0.0
    battery_charge_price_sum = 0.0
    battery_discharge_price_sum = 0.0

    turbine_steps = 0
    pump_steps = 0
    spill_steps = 0
    battery_charge_steps = 0
    battery_discharge_steps = 0
    wait_steps = 0

    for index, row in enumerate(dispatch_rows):
        price = row["price"]
        if not math.isclose(price, prices[index], abs_tol=1.0e-9):
            fail(f"dispatch price mismatch at index {index}: expected {prices[index]}, got {price}")
        if not math.isclose(row["natural_inflow"], inflows[index], abs_tol=1.0e-9):
            fail(f"dispatch inflow mismatch at index {index}: expected {inflows[index]}, got {row['natural_inflow']}")

        turbine_power = row["turbine_flow"] * water_to_power * turbine_efficiency
        pump_power = row["pump_flow"] * water_to_power / pump_efficiency
        charge_power = row["battery_charge_power"]
        discharge_power = row["battery_discharge_power"]
        net_power = row["net_power"]

        exported = max(net_power, 0.0) * dt
        imported = max(-net_power, 0.0) * dt
        turbine_energy = turbine_power * dt
        pump_energy = pump_power * dt
        charge_energy = charge_power * dt
        discharge_energy = discharge_power * dt

        export_mwh += exported
        import_mwh += imported
        export_revenue += price * exported
        import_cost += price * imported
        turbine_generation_mwh += turbine_energy
        pump_consumption_mwh += pump_energy
        battery_charge_mwh += charge_energy
        battery_discharge_mwh += discharge_energy
        operating_cost_total += operating_cost * (turbine_power + pump_power + charge_power + discharge_power) * dt
        battery_degradation_total += battery_degradation_cost * (charge_power + discharge_power) * dt
        reward_total += row["reward"]

        turbine_price_sum += price * turbine_energy
        pump_price_sum += price * pump_energy
        battery_charge_price_sum += price * charge_energy
        battery_discharge_price_sum += price * discharge_energy

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
        if charge_power > 0.0:
            battery_charge_steps += 1
            active = True
        if discharge_power > 0.0:
            battery_discharge_steps += 1
            active = True
        if not active:
            wait_steps += 1

    first = dispatch_rows[0] if dispatch_rows else None
    last = dispatch_rows[-1] if dispatch_rows else None
    if first is None or last is None:
        fail("dispatch file is empty")

    net_market_cashflow = export_revenue - import_cost
    recomputed_reward = net_market_cashflow - operating_cost_total - battery_degradation_total
    reported_cumulative_profit = last["cumulative_profit"]

    print("OptiFlow dispatch summary")
    print_metric("Scenario", params.get("scenario_name", ""))
    print_metric("Rows", len(dispatch_rows))
    print_metric("Time step hours", dt)
    print("")
    print_metric("Export revenue", export_revenue)
    print_metric("Import cost", import_cost)
    print_metric("Net market cashflow", net_market_cashflow)
    print_metric("Operating cost", operating_cost_total)
    print_metric("Battery degradation cost", battery_degradation_total)
    print_metric("Recomputed reward", recomputed_reward)
    print_metric("Dispatch reward sum", reward_total)
    print_metric("Reported cumulative profit", reported_cumulative_profit)
    print_metric("Reward difference", reported_cumulative_profit - reward_total)
    print("")
    print_metric("Export energy", export_mwh, "MWh")
    print_metric("Import energy", import_mwh, "MWh")
    print_metric("Net export energy", export_mwh - import_mwh, "MWh")
    print_metric("Turbine generation", turbine_generation_mwh, "MWh")
    print_metric("Pump consumption", pump_consumption_mwh, "MWh")
    print_metric("Battery charge energy", battery_charge_mwh, "MWh")
    print_metric("Battery discharge energy", battery_discharge_mwh, "MWh")
    print("")
    print_metric("Average export price", weighted_average_price(export_revenue, export_mwh))
    print_metric("Average import price", weighted_average_price(import_cost, import_mwh))
    print_metric("Average turbine price", weighted_average_price(turbine_price_sum, turbine_generation_mwh))
    print_metric("Average pump price", weighted_average_price(pump_price_sum, pump_consumption_mwh))
    print_metric("Average battery charge price", weighted_average_price(battery_charge_price_sum, battery_charge_mwh))
    print_metric("Average battery discharge price", weighted_average_price(battery_discharge_price_sum, battery_discharge_mwh))
    print("")
    print_metric("Turbine steps", turbine_steps)
    print_metric("Pump steps", pump_steps)
    print_metric("Spill steps", spill_steps)
    print_metric("Battery charge steps", battery_charge_steps)
    print_metric("Battery discharge steps", battery_discharge_steps)
    print_metric("Wait steps", wait_steps)
    print("")
    print_metric("Initial reservoir volume", first["reservoir_volume"])
    print_metric("Final reservoir volume", last["next_reservoir_volume"])
    print_metric("Terminal reservoir target", terminal_reservoir_target)
    print_metric("Terminal reservoir target deviation", last["next_reservoir_volume"] - terminal_reservoir_target)
    print_metric("Initial battery SOC", first["battery_soc"])
    print_metric("Final battery SOC", last["next_battery_soc"])
    print_metric("Terminal battery target", terminal_battery_target)
    print_metric("Terminal battery target deviation", last["next_battery_soc"] - terminal_battery_target)


def main() -> int:
    args = parse_args()
    try:
        summarize(args)
    except SummaryError as error:
        print(f"Dispatch summary failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
