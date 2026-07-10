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

test("buildTraderRows partitions weekday peak and off-peak hours", () => {
  const rows = buildTraderRows(dispatchCsv(24), reporting);
  const baseload = rows.find((row) => row.product === "Baseload");
  const peak = rows.find((row) => row.product === "Peak");
  const offPeak = rows.find((row) => row.product === "Off-peak");

  assert.deepEqual(
    { hours: baseload.hours, averageMw: baseload.averageMw, energyMwh: baseload.energyMwh, pnl: baseload.pnl },
    { hours: 24, averageMw: 1, energyMwh: 24, pnl: 48 },
  );
  assert.deepEqual(
    { hours: peak.hours, energyMwh: peak.energyMwh, pnl: peak.pnl },
    { hours: 11, energyMwh: 11, pnl: 22 },
  );
  assert.deepEqual(
    { hours: offPeak.hours, energyMwh: offPeak.energyMwh, pnl: offPeak.pnl },
    { hours: 13, energyMwh: 13, pnl: 26 },
  );
});

test("buildTraderRows uses monthly periods first and calendar quarters later", () => {
  const rows = buildTraderRows(
    dispatchCsv(370),
    { ...reporting, market_start_utc: "2027-01-01T00:00:00Z", time_step_hours: 24 },
  );
  const periods = [...new Set(rows.map((row) => row.period))];

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
