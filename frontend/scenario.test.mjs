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
      .map((field) => [field.key, String(field.defaultValue)]),
  );
}

test("buildScenarioCsv emits every required scalar parameter", () => {
  const csv = buildScenarioCsv("custom_case", defaultValues());
  const lines = csv.trimEnd().split("\n");

  assert.equal(lines[0], "key,value");
  assert.equal(lines[1], "scenario_name,custom_case");
  assert.equal(lines.length, 2 + SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields).length);
  assert.match(csv, /terminal_target_reservoir_volume,250/);
  assert.match(csv, /discount_factor,1/);
});

test("buildScenarioCsv rejects names that cannot be represented by the parser", () => {
  assert.throws(
    () => buildScenarioCsv("bad,name", defaultValues()),
    /cannot contain commas/,
  );
});

test("buildScenarioCsv validates cross-field bounds", () => {
  const values = defaultValues();
  values.initial_reservoir_volume = "900";

  assert.throws(
    () => buildScenarioCsv("bad_initial_state", values),
    /Initial reservoir volume/,
  );
});

test("buildScenarioCsv requires integer solver counts", () => {
  const values = defaultValues();
  values.turbine_flow_steps = "2.5";

  assert.throws(
    () => buildScenarioCsv("bad_steps", values),
    /must be an integer/,
  );
});

test("validateSeriesCsv accepts negative prices and sequential indices", () => {
  const result = validateSeriesCsv(
    "time_index,price\n0,-10\n1,42.5\n",
    "price",
  );

  assert.equal(result.rowCount, 2);
});

test("validateSeriesCsv rejects negative inflows", () => {
  assert.throws(
    () => validateSeriesCsv(
      "time_index,natural_inflow\n0,3\n1,-1\n",
      "natural_inflow",
    ),
    /negative natural_inflow/,
  );
});

test("validateSeriesCsv rejects a non-sequential index", () => {
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
