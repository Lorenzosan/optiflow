import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

import { abbreviateSha256, provenanceItems } from "./run_presentation.mjs";

const HASHES = Object.freeze({
  scenario: "a".repeat(64),
  prices: "b".repeat(64),
  inflows: "c".repeat(64),
  solver: "d".repeat(64),
  dispatch: "e".repeat(64),
});

function sampleProvenance(overrides = {}) {
  return {
    result_schema_version: 1,
    scenario_sha256: HASHES.scenario,
    prices_sha256: HASHES.prices,
    inflows_sha256: HASHES.inflows,
    solver_sha256: HASHES.solver,
    dispatch_sha256: HASHES.dispatch,
    horizon_steps: 8760,
    reservoir_volume_grid_points: 33,
    turbine_flow_steps: 9,
    pump_flow_steps: 9,
    spill_flow_steps: 9,
    ...overrides,
  };
}

test("abbreviateSha256 preserves identifying prefix and suffix", () => {
  const hash = "0123456789abcdef".repeat(4);
  assert.equal(abbreviateSha256(hash), "0123456789ab…89abcdef");
  assert.equal(abbreviateSha256("not-a-hash"), "Unavailable");
});

test("provenanceItems exposes configuration and abbreviated hashes", () => {
  const items = provenanceItems(sampleProvenance());
  const values = Object.fromEntries(items.map((item) => [item.label, item.value]));

  assert.equal(values["Result schema"], "v1");
  assert.equal(values.Horizon, "8760 steps");
  assert.equal(values["State grid"], "33 storage points");
  assert.equal(values["Action steps"], "Turbine 9 · Pump 9 · Spill 9");
  assert.equal(values["Scenario SHA-256"], "aaaaaaaaaaaa…aaaaaaaa");
  assert.equal(items.find((item) => item.label === "Solver SHA-256").fullValue, HASHES.solver);
});

test("provenanceItems distinguishes a failed run without a dispatch", () => {
  const items = provenanceItems(sampleProvenance({ dispatch_sha256: null }));
  assert.equal(
    items.find((item) => item.label === "Dispatch SHA-256").value,
    "Not produced",
  );
});

test("provenanceItems rejects malformed hashes only in presentation", () => {
  const items = provenanceItems(sampleProvenance({ dispatch_sha256: "invalid" }));
  assert.equal(
    items.find((item) => item.label === "Dispatch SHA-256").value,
    "Unavailable",
  );
});

test("provenanceItems degrades malformed configuration without breaking the run view", () => {
  const items = provenanceItems(sampleProvenance({ horizon_steps: 0 }));
  assert.equal(items.find((item) => item.label === "Horizon").value, "Unavailable");
});

test("provenanceItems returns no rows for historical runs without provenance", () => {
  assert.deepEqual(provenanceItems(null), []);
});

test("selected-run assets wire the provenance panel", () => {
  const appSource = readFileSync(new URL("./app.js", import.meta.url), "utf8");
  const indexHtml = readFileSync(new URL("./index.html", import.meta.url), "utf8");

  assert.match(appSource, /renderProvenance\(run\.provenance\)/);
  assert.match(indexHtml, /id="run-provenance"/);
  assert.match(indexHtml, /id="provenance-grid"/);
});
