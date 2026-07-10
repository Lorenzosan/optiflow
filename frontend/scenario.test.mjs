import assert from "node:assert/strict";
import test from "node:test";

import {
  SCENARIO_PARAMETER_GROUPS,
  buildScenarioCsv,
  validateSeriesCsv,
  validateSeriesPair,
} from "./scenario.mjs";

function defaultValues() {
  return {
    ...Object.fromEntries(
      SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields)
        .map((definition) => [definition.key, String(definition.defaultValue)]),
    ),
    series_start_utc: "2026-12-31T23:00:00Z",
  };
}

test("buildScenarioCsv emits the reservoir-only schema", () => {
  const csv = buildScenarioCsv("custom_case", defaultValues());
  const lines = csv.trimEnd().split("\n");
  assert.equal(lines[0], "key,value");
  assert.equal(lines[1], "scenario_name,custom_case");
  assert.equal(
    lines.length,
    3 + SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).length,
  );
  assert.match(csv, /terminal_target_reservoir_volume,250/);
  assert.match(csv, /discount_factor,1/);
  assert.equal(lines[2], "series_start_utc,2026-12-31T23:00:00Z");
  assert.deepEqual(
    lines.slice(3).map((line) => line.split(",", 1)[0]),
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

test("validateSeriesCsv accepts negative prices", () => {
  assert.equal(validateSeriesCsv("time_index,price\n0,-10\n1,42.5\n", "price").rowCount, 2);
});

test("validateSeriesCsv rejects negative inflows", () => {
  assert.throws(
    () => validateSeriesCsv("time_index,natural_inflow\n0,3\n1,-1\n", "natural_inflow"),
    /negative natural_inflow/,
  );
});

test("validateSeriesCsv rejects non-sequential indices", () => {
  assert.throws(
    () => validateSeriesCsv("time_index,price\n0,1\n2,3\n", "price"),
    /must use time_index 1/,
  );
});

test("validateSeriesPair rejects mismatched horizons", () => {
  assert.throws(
    () => validateSeriesPair(
      "time_index,price\n0,1\n1,2\n",
      "time_index,natural_inflow\n0,3\n",
    ),
    /horizons differ/,
  );
});

test("buildScenarioCsv rejects an invalid series start", () => {
  const values = defaultValues();
  values.series_start_utc = "2027-01-01T00:00:00+01:00";
  assert.throws(() => buildScenarioCsv("bad_start", values), /must be an ISO-8601 UTC datetime/);
});
