import assert from "node:assert/strict";
import test from "node:test";

import { buildTraderRows, parseDispatchCsv } from "./trader.mjs";

function dispatchCsv(rowCount, netPower = 1, reward = 2) {
  const rows = ["time_index,net_power,reward"];
  for (let index = 0; index < rowCount; index += 1) {
    rows.push(`${index},${netPower},${reward}`);
  }
  return `${rows.join("\n")}\n`;
}

const reporting = {
  market_start_utc: "2027-01-04T00:00:00Z",
  market_timezone: "UTC",
  peak_start_hour: 9,
  peak_end_hour: 20,
  time_step_hours: 1,
};

test("buildTraderRows returns one period row with product metric columns", () => {
  const rows = buildTraderRows(dispatchCsv(24), reporting);

  assert.equal(rows.length, 1);
  assert.equal(rows[0].period, "January 2027");
  assert.deepEqual(
    rows[0].baseload,
    { averageMw: 1, energyMwh: 24, pnl: 48 },
  );
  assert.deepEqual(
    rows[0].peak,
    { averageMw: 1, energyMwh: 11, pnl: 22 },
  );
  assert.deepEqual(
    rows[0].offPeak,
    { averageMw: 1, energyMwh: 13, pnl: 26 },
  );
  assert.equal("hours" in rows[0].baseload, false);
});

test("buildTraderRows reports an unavailable average for an empty product", () => {
  const rows = buildTraderRows(
    dispatchCsv(2, -3, -4),
    { ...reporting, market_start_utc: "2027-01-09T00:00:00Z" },
  );

  assert.equal(rows[0].peak.averageMw, null);
  assert.equal(rows[0].peak.energyMwh, 0);
  assert.equal(rows[0].peak.pnl, 0);
  assert.deepEqual(
    rows[0].offPeak,
    { averageMw: -3, energyMwh: -6, pnl: -8 },
  );
});

test("buildTraderRows uses monthly periods first and calendar quarters later", () => {
  const rows = buildTraderRows(
    dispatchCsv(370),
    { ...reporting, market_start_utc: "2027-01-01T00:00:00Z", time_step_hours: 24 },
  );
  const periods = rows.map((row) => row.period);

  assert.equal(periods[0], "January 2027");
  assert.ok(periods.includes("December 2027"));
  assert.ok(periods.includes("Q1 2028"));
});

test("parseDispatchCsv rejects non-sequential time indices", () => {
  assert.throws(
    () => parseDispatchCsv("time_index,net_power,reward\n0,1,2\n2,1,2\n"),
    /time_index 1/,
  );
});
