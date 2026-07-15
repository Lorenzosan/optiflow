import assert from "node:assert/strict";
import test from "node:test";

import { buildTraderRows, parseDispatchCsv } from "./trader.mjs";

function dispatchCsv(rowCount, {
  start = "2027-01-03T23:00:00Z",
  timeStepHours = 1,
  netPower = 1,
  reward = 2,
} = {}) {
  const rows = ["time_index,timestamp_utc,net_power,reward"];
  const startMilliseconds = Date.parse(start);
  for (let index = 0; index < rowCount; index += 1) {
    const timestamp = new Date(startMilliseconds + index * timeStepHours * 3_600_000)
      .toISOString()
      .replace(".000Z", "Z");
    rows.push(`${index},${timestamp},${netPower},${reward}`);
  }
  return `${rows.join("\n")}\n`;
}

test("buildTraderRows returns one period row with fixed Zurich products", () => {
  const rows = buildTraderRows(dispatchCsv(24), 1);

  assert.equal(rows.length, 1);
  assert.equal(rows[0].period, "January 2027");
  assert.deepEqual(rows[0].baseload, { averageMw: 1, energyMwh: 24, cashflow: 48 });
  assert.deepEqual(rows[0].peak, { averageMw: 1, energyMwh: 11, cashflow: 22 });
  assert.deepEqual(rows[0].offPeak, { averageMw: 1, energyMwh: 13, cashflow: 26 });
  assert.equal("hours" in rows[0].baseload, false);
});

test("buildTraderRows classifies timestamps in Europe/Zurich", () => {
  const rows = buildTraderRows(dispatchCsv(1, { start: "2027-01-04T08:00:00Z" }), 1);

  assert.equal(rows[0].peak.energyMwh, 1);
  assert.equal(rows[0].offPeak.energyMwh, 0);
});

test("buildTraderRows splits optimization intervals at peak boundaries", () => {
  const rows = buildTraderRows(
    dispatchCsv(1, { start: "2027-01-04T07:00:00Z", timeStepHours: 2, netPower: 2, reward: 20 }),
    2,
  );

  assert.deepEqual(rows[0].baseload, { averageMw: 2, energyMwh: 4, cashflow: 20 });
  assert.deepEqual(rows[0].peak, { averageMw: 2, energyMwh: 2, cashflow: 10 });
  assert.deepEqual(rows[0].offPeak, { averageMw: 2, energyMwh: 2, cashflow: 10 });
});

test("buildTraderRows reports an unavailable average for an empty product", () => {
  const rows = buildTraderRows(
    dispatchCsv(2, { start: "2027-01-08T23:00:00Z", netPower: -3, reward: -4 }),
    1,
  );

  assert.equal(rows[0].peak.averageMw, null);
  assert.equal(rows[0].peak.energyMwh, 0);
  assert.equal(rows[0].peak.cashflow, 0);
  assert.deepEqual(rows[0].offPeak, { averageMw: -3, energyMwh: -6, cashflow: -8 });
});

test("buildTraderRows uses monthly periods first and calendar quarters later", () => {
  const rows = buildTraderRows(
    dispatchCsv(370, { start: "2026-12-31T23:00:00Z", timeStepHours: 24 }),
    24,
  );
  const periods = rows.map((row) => row.period);

  assert.equal(periods[0], "January 2027");
  assert.ok(periods.includes("December 2027"));
  assert.ok(periods.includes("Q1 2028"));
});

test("parseDispatchCsv rejects missing or non-sequential timestamps", () => {
  assert.throws(
    () => parseDispatchCsv("time_index,net_power,reward\n0,1,2\n"),
    /missing timestamp_utc/,
  );
  assert.throws(
    () => parseDispatchCsv(
      "time_index,timestamp_utc,net_power,reward\n"
      + "0,2027-01-01T01:00:00Z,1,2\n"
      + "1,2027-01-01T00:00:00Z,1,2\n",
    ),
    /strictly increasing/,
  );
});

test("buildTraderRows rejects dispatch spacing that differs from the optimizer step", () => {
  assert.throws(
    () => buildTraderRows(dispatchCsv(2, { timeStepHours: 2 }), 1),
    /does not match time_step_hours/,
  );
});
