import assert from "node:assert/strict";
import test from "node:test";

import {
  SCENARIO_PARAMETER_GROUPS,
  buildScenarioCsv,
  parseScenarioCsv,
  suggestScenarioCopyName,
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


test("scenario editor exposes explicit hydro and euro units without a time-step input", () => {
  const fields = Object.fromEntries(
    SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields)
      .map((definition) => [definition.key, definition.label]),
  );

  assert.equal(fields.time_step_hours, undefined);
  assert.equal(fields.reservoir_max_volume, "Maximum storage content [MWh hydraulic]");
  assert.equal(fields.turbine_max_flow, "Maximum turbine withdrawal [MW hydraulic]");
  assert.equal(fields.turbine_efficiency, "Turbine efficiency [%]");
  assert.equal(fields.pump_efficiency, "Pump efficiency [%]");
  assert.equal(fields.water_to_power_factor, undefined);
  assert.equal(fields.operating_cost_per_mwh, "Operating cost [€/MWh]");
  assert.equal(fields.terminal_reservoir_target_penalty, "Storage target penalty [€/MWh²]");
});

test("buildScenarioCsv emits the derived time step and optimizer scalar schema", () => {
  const csv = buildScenarioCsv("custom_case", defaultValues(), 0.5);
  const lines = csv.trimEnd().split("\n");
  assert.equal(lines[0], "key,value");
  assert.equal(lines[1], "scenario_name,custom_case");
  assert.equal(lines[2], "time_step_hours,0.5");
  assert.equal(
    lines.length,
    3 + SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).length,
  );
  assert.match(csv, /terminal_target_reservoir_volume,100/);
  assert.match(csv, /turbine_efficiency,0\.9/);
  assert.match(csv, /pump_efficiency,0\.85/);
  assert.match(csv, /discount_factor,1/);
  assert.doesNotMatch(csv, /market_|peak_|series_start|water_to_power_factor/);
  assert.deepEqual(
    lines.slice(3).map((line) => line.split(",", 1)[0]),
    SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).map((field) => field.key),
  );
});

test("parseScenarioCsv hydrates editor values and restores efficiency percentages", () => {
  const csv = buildScenarioCsv("loaded_case", defaultValues(), 0.5);
  const parsed = parseScenarioCsv(csv);

  assert.equal(parsed.name, "loaded_case");
  assert.equal(parsed.timeStepHours, 0.5);
  assert.equal(parsed.values.turbine_efficiency, 90);
  assert.equal(parsed.values.pump_efficiency, 85);
  assert.equal(parsed.values.reservoir_max_volume, 200);
});

test("parseScenarioCsv rejects incomplete or unsupported stored inputs", () => {
  assert.throws(
    () => parseScenarioCsv("key,value\nscenario_name,incomplete\ntime_step_hours,1\n"),
    /missing:/,
  );
  assert.throws(
    () => parseScenarioCsv(`${buildScenarioCsv("extra", defaultValues(), 1)}unknown_key,2\n`),
    /unsupported key/,
  );
});

test("suggestScenarioCopyName avoids existing scenario names", () => {
  assert.equal(suggestScenarioCopyName("sample", []), "sample_copy");
  assert.equal(
    suggestScenarioCopyName("sample", ["sample_copy", "sample_copy_2"]),
    "sample_copy_3",
  );
  assert.equal(suggestScenarioCopyName("x".repeat(128), []).length, 128);
});

test("buildScenarioCsv converts displayed efficiency percentages to fractions", () => {
  const values = defaultValues();
  values.turbine_efficiency = "87.5";
  values.pump_efficiency = "92";
  const csv = buildScenarioCsv("efficiency_case", values, 1);

  assert.match(csv, /turbine_efficiency,0\.875/);
  assert.match(csv, /pump_efficiency,0\.92/);
});

test("buildScenarioCsv validates displayed efficiency percentages", () => {
  const zeroEfficiency = defaultValues();
  zeroEfficiency.turbine_efficiency = "0";
  assert.throws(
    () => buildScenarioCsv("zero_efficiency", zeroEfficiency, 1),
    /below its allowed minimum/,
  );

  const excessiveEfficiency = defaultValues();
  excessiveEfficiency.pump_efficiency = "100.1";
  assert.throws(
    () => buildScenarioCsv("excessive_efficiency", excessiveEfficiency, 1),
    /exceeds its allowed maximum/,
  );
});

test("buildScenarioCsv rejects invalid derived time steps", () => {
  assert.throws(
    () => buildScenarioCsv("bad_time_step", defaultValues(), 0),
    /Derived time step/,
  );
});

test("buildScenarioCsv rejects unrepresentable names", () => {
  assert.throws(
    () => buildScenarioCsv("bad,name", defaultValues(), 1),
    /cannot contain commas/,
  );
});

test("buildScenarioCsv validates reservoir bounds", () => {
  const values = defaultValues();
  values.initial_reservoir_volume = "900";
  assert.throws(
    () => buildScenarioCsv("bad_initial", values, 1),
    /Initial storage content/,
  );
});

test("buildScenarioCsv requires integer solver counts", () => {
  const values = defaultValues();
  values.turbine_flow_steps = "2.5";
  assert.throws(
    () => buildScenarioCsv("bad_steps", values, 1),
    /must be an integer/,
  );
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
    ),
    /timestamps differ/,
  );
});

test("validateSeriesPair derives a constant time step from timestamps", () => {
  const oneHour = validateSeriesPair(PRICE_CSV, INFLOW_CSV);
  assert.equal(oneHour.rowCount, 2);
  assert.equal(oneHour.timeStepHours, 1);

  const twoHourPrices = PRICE_CSV.replace("T01:00:00Z", "T02:00:00Z");
  const twoHourInflows = INFLOW_CSV.replace("T01:00:00Z", "T02:00:00Z");
  assert.equal(validateSeriesPair(twoHourPrices, twoHourInflows).timeStepHours, 2);
});

test("validateSeriesPair rejects irregular spacing", () => {
  const prices = [
    "timestamp_utc,price",
    "2027-01-01T00:00:00Z,1",
    "2027-01-01T01:00:00Z,2",
    "2027-01-01T03:00:00Z,3",
    "",
  ].join("\n");
  const inflows = prices.replace("price", "natural_inflow");
  assert.throws(() => validateSeriesPair(prices, inflows), /not constant/);
});

test("validateSeriesPair requires two rows to infer the time step", () => {
  assert.throws(
    () => validateSeriesPair(
      "timestamp_utc,price\n2027-01-01T00:00:00Z,1\n",
      "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,1\n",
    ),
    /At least two/,
  );
});

test("validateSeriesPair rejects mismatched horizons", () => {
  assert.throws(
    () => validateSeriesPair(
      PRICE_CSV,
      "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,3\n",
    ),
    /horizons differ/,
  );
});
