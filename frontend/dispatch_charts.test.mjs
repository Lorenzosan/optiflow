import assert from "node:assert/strict";
import test from "node:test";

import {
  buildDispatchChartModel,
  buildSeriesPath,
  clampTimeDomain,
  extentForPanel,
  panTimeDomain,
  tooltipSelectionAtTimestamp,
  zoomTimeDomain,
} from "./dispatch_charts.mjs";

const HEADER = "time_index,timestamp_utc,price,natural_inflow,reservoir_volume,turbine_flow,spill_flow,pump_flow,next_reservoir_volume,net_power,reward";

function dispatchCsv(rows) {
  return `${[HEADER, ...rows].join("\n")}\n`;
}

test("buildDispatchChartModel creates aligned interval and boundary series", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,30,0,0,0,0,10,10,-12.5,-400",
    "1,2027-01-04T01:00:00Z,60,0,10,10,0,0,0,8,464",
  ]), 1);

  assert.equal(model.rows.length, 2);
  assert.equal(model.stepMilliseconds, 3_600_000);
  assert.equal(model.endMilliseconds - model.startMilliseconds, 7_200_000);

  const price = model.panels.find((panel) => panel.key === "price");
  assert.equal(price.unit, "€/MWh");
  assert.deepEqual(price.series[0].points.map((point) => point.value), [30, 60, 60]);

  assert.deepEqual(
    model.panels.map((panel) => panel.key),
    ["price", "inflow", "turbine", "pump", "spill", "reservoir", "cashflow", "operating-cost"],
  );
  const pump = model.panels.find((panel) => panel.key === "pump");
  assert.equal(pump.unit, "MW hydraulic");
  assert.deepEqual(pump.series[0].points.map((point) => point.value), [10, 0, 0]);

  const reservoir = model.panels.find((panel) => panel.key === "reservoir");
  assert.equal(reservoir.title, "Storage content");
  assert.equal(reservoir.unit, "MWh hydraulic");
  assert.equal(reservoir.series[0].label, "Storage content");
  assert.deepEqual(reservoir.series[0].points.map((point) => point.value), [0, 10, 0]);

  const cashflow = model.panels.find((panel) => panel.key === "cashflow");
  assert.equal(cashflow.unit, "€ / interval");
  assert.equal(cashflow.series.length, 1);
  assert.equal(cashflow.series[0].label, "Net operating cashflow");
  assert.deepEqual(cashflow.series[0].points.map((point) => point.value), [-400, 464, 464]);

  const operatingCost = model.panels.find((panel) => panel.key === "operating-cost");
  assert.equal(operatingCost.unit, "€ / interval");
  assert.equal(operatingCost.series.length, 1);
  assert.equal(operatingCost.series[0].label, "Operating cost");
  assert.deepEqual(operatingCost.series[0].points.map((point) => point.value), [25, 16, 16]);
});

test("zero operating cost does not hide interval cashflow", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,0,0,16,0,0,0,16,0,0",
    "1,2027-01-04T01:00:00Z,1,0,16,16,0,0,0,14.4,14.4",
  ]), 1);

  const cashflow = model.panels.find((panel) => panel.key === "cashflow");
  const operatingCost = model.panels.find((panel) => panel.key === "operating-cost");
  assert.deepEqual(cashflow.series[0].points.map((point) => point.value), [0, 14.4, 14.4]);
  assert.deepEqual(operatingCost.series[0].points.map((point) => point.value), [0, 0, 0]);
  assert.notEqual(cashflow, operatingCost);
});



test("tooltip selection exposes the terminal storage boundary", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,0,0,16,0,0,0,16,0,0",
    "1,2027-01-04T01:00:00Z,1,0,16,16,0,0,0,14.4,14.4",
  ]), 1);

  const interval = tooltipSelectionAtTimestamp(model, Date.parse("2027-01-04T01:30:00Z"));
  assert.equal(interval.kind, "interval");
  assert.equal(interval.row.timeIndex, 1);

  const terminal = tooltipSelectionAtTimestamp(model, model.endMilliseconds);
  assert.deepEqual(terminal, {
    kind: "terminal",
    timestampMilliseconds: Date.parse("2027-01-04T02:00:00Z"),
    storageContent: 0,
  });
});

test("buildDispatchChartModel rejects timestamp spacing inconsistent with the scenario", () => {
  assert.throws(
    () => buildDispatchChartModel(dispatchCsv([
      "0,2027-01-04T00:00:00Z,30,0,0,0,0,0,0,0,0",
      "1,2027-01-04T02:00:00Z,60,0,0,0,0,0,0,0,0",
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



test("hydraulic flows use separate raw positive panels", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,30,4,300,12,0,0,292,10,100",
    "1,2027-01-04T01:00:00Z,20,4,292,0,3,8,301,-8,-50",
    "2,2027-01-04T02:00:00Z,40,4,301,0,0,0,305,0,0",
  ]), 1);

  const turbine = model.panels.find((panel) => panel.key === "turbine");
  const pump = model.panels.find((panel) => panel.key === "pump");
  const spill = model.panels.find((panel) => panel.key === "spill");

  assert.equal(turbine.series.length, 1);
  assert.equal(pump.series.length, 1);
  assert.equal(spill.series.length, 1);
  assert.deepEqual(turbine.series[0].points.map((point) => point.value), [12, 0, 0, 0]);
  assert.deepEqual(pump.series[0].points.map((point) => point.value), [0, 8, 0, 0]);
  assert.deepEqual(spill.series[0].points.map((point) => point.value), [0, 3, 0, 0]);
  assert.equal(turbine.height, 72);
  assert.equal(pump.height, 72);
  assert.equal(spill.height, 72);
});

test("nonnegative panels keep zero as the lower axis bound", () => {
  const model = buildDispatchChartModel(dispatchCsv([
    "0,2027-01-04T00:00:00Z,30,4,0,0,0,0,4,0,0",
    "1,2027-01-04T01:00:00Z,60,8,4,0,0,0,12,0,0",
  ]), 1);
  const inflow = model.panels.find((panel) => panel.key === "inflow");

  assert.equal(extentForPanel(inflow).minimum, 0);
});


test("zoomTimeDomain preserves the anchor and clamps to the minimum span", () => {
  assert.deepEqual(
    zoomTimeDomain(0, 100, 25, 0.5, 0, 100, 10),
    { startMilliseconds: 12.5, endMilliseconds: 62.5 },
  );
  assert.deepEqual(
    zoomTimeDomain(0, 100, 50, 0.01, 0, 100, 10),
    { startMilliseconds: 45, endMilliseconds: 55 },
  );
});

test("panTimeDomain keeps the shared window inside the full dispatch range", () => {
  assert.deepEqual(
    panTimeDomain(20, 60, 30, 0, 100),
    { startMilliseconds: 50, endMilliseconds: 90 },
  );
  assert.deepEqual(
    panTimeDomain(20, 60, 80, 0, 100),
    { startMilliseconds: 60, endMilliseconds: 100 },
  );
  assert.deepEqual(
    panTimeDomain(20, 60, -80, 0, 100),
    { startMilliseconds: 0, endMilliseconds: 40 },
  );
});

test("clampTimeDomain expands short windows without exceeding chart bounds", () => {
  assert.deepEqual(
    clampTimeDomain(96, 98, 0, 100, 10),
    { startMilliseconds: 90, endMilliseconds: 100 },
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

test("extentForPanel ignores negligible negative floating-point noise", () => {
  const extent = extentForPanel({
    includeZero: true,
    series: [{
      points: [
        { value: -1e-12 },
        { value: 194 },
      ],
    }],
  });

  assert.equal(extent.minimum, 0);
  assert.ok(extent.maximum > 194);
});
