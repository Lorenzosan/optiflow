import assert from "node:assert/strict";
import test from "node:test";

import {
  SCENARIO_PARAMETER_GROUPS,
  buildScenarioCsv,
  validateSeriesCsv,
  validateSeriesPair,
} from "./scenario.mjs";

function defaultValues() {
  return Object.fromEntries(
    SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields)
      .map((definition) => [definition.key, String(definition.defaultValue)]),
  );
}

const PRICE_CSV = [
  "timestamp_utc,price",
  "2027-01-01T00:00:00Z,-10",
  "2027-01-01T01:00:00Z,42.5",
  "",
].join("\n");

const INFLOW_CSV = [
  "timestamp_utc,natural_inflow",
  "2027-01-01T00:00:00Z,3",
  "2027-01-01T01:00:00Z,1",
  "",
].join("\n");


test("scenario editor exposes explicit hydro and euro units", () => {
  const fields = Object.fromEntries(
    SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields)
      .map((definition) => [definition.key, definition.label]),
  );

  assert.equal(fields.reservoir_max_volume, "Reservoir maximum [10³ m³]");
  assert.equal(fields.turbine_max_flow, "Maximum turbine flow [10³ m³/h]");
  assert.equal(fields.water_to_power_factor, "Water-to-power factor [MW/(10³ m³/h)]");
  assert.equal(fields.operating_cost_per_mwh, "Operating cost [€/MWh]");
  assert.equal(fields.terminal_reservoir_target_penalty, "Reservoir target penalty [€/(10³ m³)²]");
});

test("buildScenarioCsv emits only the optimizer scalar schema", () => {
  const csv = buildScenarioCsv("custom_case", defaultValues());
  const lines = csv.trimEnd().split("\n");
  assert.equal(lines[0], "key,value");
  assert.equal(lines[1], "scenario_name,custom_case");
  assert.equal(
    lines.length,
    2 + SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).length,
  );
  assert.match(csv, /terminal_target_reservoir_volume,250/);
  assert.match(csv, /discount_factor,1/);
  assert.doesNotMatch(csv, /market_|peak_|series_start/);
  assert.deepEqual(
    lines.slice(2).map((line) => line.split(",", 1)[0]),
    SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).map((field) => field.key),
  );
});

test("buildScenarioCsv rejects unrepresentable names", () => {
  assert.throws(() => buildScenarioCsv("bad,name", defaultValues()), /cannot contain commas/);
});

test("buildScenarioCsv validates reservoir bounds", () => {
  const values = defaultValues();
  values.initial_reservoir_volume = "900";
  assert.throws(() => buildScenarioCsv("bad_initial", values), /Initial reservoir volume/);
});

test("buildScenarioCsv requires integer solver counts", () => {
  const values = defaultValues();
  values.turbine_flow_steps = "2.5";
  assert.throws(() => buildScenarioCsv("bad_steps", values), /must be an integer/);
});

test("validateSeriesCsv accepts canonical UTC timestamps and negative prices", () => {
  const result = validateSeriesCsv(PRICE_CSV, "price");
  assert.equal(result.rowCount, 2);
  assert.equal(result.timestamps[0].text, "2027-01-01T00:00:00Z");
});

test("validateSeriesCsv rejects malformed or invalid timestamps", () => {
  assert.throws(
    () => validateSeriesCsv(
      "timestamp_utc,price\n2027-01-01T00:00:00+00:00,1\n",
      "price",
    ),
    /YYYY-MM-DDTHH:MM:SSZ/,
  );
  assert.throws(
    () => validateSeriesCsv(
      "timestamp_utc,price\n2027-02-30T00:00:00Z,1\n",
      "price",
    ),
    /invalid timestamp_utc/,
  );
});

test("validateSeriesCsv rejects non-increasing timestamps", () => {
  assert.throws(
    () => validateSeriesCsv(
      "timestamp_utc,price\n2027-01-01T01:00:00Z,1\n2027-01-01T00:00:00Z,2\n",
      "price",
    ),
    /strictly increasing/,
  );
});

test("validateSeriesCsv rejects negative inflows", () => {
  assert.throws(
    () => validateSeriesCsv(
      "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,3\n2027-01-01T01:00:00Z,-1\n",
      "natural_inflow",
    ),
    /negative natural_inflow/,
  );
});

test("validateSeriesPair requires identical timestamps", () => {
  assert.throws(
    () => validateSeriesPair(
      PRICE_CSV,
      "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,3\n2027-01-01T02:00:00Z,1\n",
      1,
    ),
    /timestamps differ/,
  );
});

test("validateSeriesPair enforces spacing from time_step_hours", () => {
  const twoHourPrices = PRICE_CSV.replace("T01:00:00Z", "T02:00:00Z");
  const twoHourInflows = INFLOW_CSV.replace("T01:00:00Z", "T02:00:00Z");
  assert.throws(
    () => validateSeriesPair(twoHourPrices, twoHourInflows, 1),
    /must equal time_step_hours/,
  );
  assert.equal(validateSeriesPair(twoHourPrices, twoHourInflows, 2).rowCount, 2);
});

test("validateSeriesPair rejects mismatched horizons", () => {
  assert.throws(
    () => validateSeriesPair(
      PRICE_CSV,
      "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,3\n",
      1,
    ),
    /horizons differ/,
  );
});
