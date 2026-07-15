import assert from "node:assert/strict";
import test from "node:test";

import { formatNumber } from "./number_format.mjs";

test("formatNumber does not display negative zero", () => {
  assert.equal(formatNumber(-0, 2, "en-US"), "0");
  assert.equal(formatNumber(-0.0001, 2, "en-US"), "0");
  assert.equal(formatNumber(-0.0049, 2, "en-US"), "0");
  assert.equal(formatNumber(-0.0051, 2, "en-US"), "-0.01");
});

test("formatNumber preserves regular values and missing values", () => {
  assert.equal(formatNumber(1234.567, 2, "en-US"), "1,234.57");
  assert.equal(formatNumber(null, 2, "en-US"), "—");
});
