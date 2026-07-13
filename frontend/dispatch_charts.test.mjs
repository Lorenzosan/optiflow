import assert from "node:assert/strict";
import test from "node:test";

import { buildDispatchChartModel, buildSeriesPath } from "./dispatch_charts.mjs";

const HEADER = "time_index,timestamp_utc,price,natural_inflow,reservoir_volume,turbine_flow,spill_flow,pump_flow,next_reservoir_volume,net_power,reward,cumulative_profit";

function dispatchCsv(rows) {
  return `${[HEADER, ...rows].join("\n")}\n`;
}

test("buildDispatchChartModel creates aligned interval and boundary series", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,30,0,0,0,0,10,10,-12.5,-400,-400",
    "1,2027-01-04T01:00:00Z,60,0,10,10,0,0,0,8,464,64",
  ]), 1);

  assert.equal(model.rows.length, 2);
  assert.equal(model.stepMilliseconds, 3_600_000);
  assert.equal(model.endMilliseconds - model.startMilliseconds, 7_200_000);

  const price = model.panels.find((panel) => panel.key === "price");
  assert.deepEqual(price.series[0].points.map((point) => point.value), [30, 60, 60]);

  const controls = model.panels.find((panel) => panel.key === "controls");
  const pump = controls.series.find((item) => item.key === "pump");
  assert.deepEqual(pump.points.map((point) => point.value), [-10, 0, 0]);

  const reservoir = model.panels.find((panel) => panel.key === "reservoir");
  assert.deepEqual(reservoir.series[0].points.map((point) => point.value), [0, 10, 0]);

  const profit = model.panels.find((panel) => panel.key === "profit");
  assert.deepEqual(profit.series[0].points.map((point) => point.value), [0, -400, 64]);
});

test("buildDispatchChartModel rejects timestamp spacing inconsistent with the scenario", () => {
  assert.throws(
    () => buildDispatchChartModel(dispatchCsv([
      "0,2027-01-04T00:00:00Z,30,0,0,0,0,0,0,0,0,0",
      "1,2027-01-04T02:00:00Z,60,0,0,0,0,0,0,0,0,0",
    ]), 1),
    /does not match time_step_hours/,
  );
});

test("buildDispatchChartModel requires the current dispatch schema", () => {
  assert.throws(
    () => buildDispatchChartModel(
      "time_index,timestamp_utc,net_power,reward\n0,2027-01-04T00:00:00Z,0,0\n",
      1,
    ),
    /missing price/,
  );
});

test("buildSeriesPath supports line and step interpolation", () => {
  const points = [
    { timestampMilliseconds: 0, value: 1 },
    { timestampMilliseconds: 10, value: 2 },
  ];
  const xScale = (value) => value;
  const yScale = (value) => value * 2;

  assert.equal(buildSeriesPath(points, "line", xScale, yScale), "M 0 2 L 10 4");
  assert.equal(buildSeriesPath(points, "step", xScale, yScale), "M 0 2 H 10 V 4");
});
