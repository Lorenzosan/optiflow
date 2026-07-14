const SHA256_PATTERN = /^[0-9a-f]{64}$/i;

function positiveInteger(value) {
  const parsed = Number(value);
  return Number.isSafeInteger(parsed) && parsed > 0 ? parsed : null;
}

export function abbreviateSha256(value) {
  if (!SHA256_PATTERN.test(String(value ?? ""))) {
    return "Unavailable";
  }
  const normalized = String(value).toLowerCase();
  return `${normalized.slice(0, 12)}…${normalized.slice(-8)}`;
}

export function provenanceItems(provenance) {
  if (!provenance) {
    return Object.freeze([]);
  }

  const resultSchemaVersion = positiveInteger(provenance.result_schema_version);
  const horizonSteps = positiveInteger(provenance.horizon_steps);
  const reservoirPoints = positiveInteger(provenance.reservoir_volume_grid_points);
  const turbineSteps = positiveInteger(provenance.turbine_flow_steps);
  const pumpSteps = positiveInteger(provenance.pump_flow_steps);
  const spillSteps = positiveInteger(provenance.spill_flow_steps);

  const hashItem = (label, value, missingValue = "Unavailable") => {
    const isHash = SHA256_PATTERN.test(String(value ?? ""));
    const isMissing = value === null || value === undefined;
    return Object.freeze({
      label,
      value: isHash ? abbreviateSha256(value) : (isMissing ? missingValue : "Unavailable"),
      fullValue: isHash ? String(value).toLowerCase() : null,
    });
  };

  const actionGrid = [
    turbineSteps === null ? "Turbine —" : `Turbine ${turbineSteps}`,
    pumpSteps === null ? "Pump —" : `Pump ${pumpSteps}`,
    spillSteps === null ? "Spill —" : `Spill ${spillSteps}`,
  ].join(" · ");

  return Object.freeze([
    Object.freeze({
      label: "Result schema",
      value: resultSchemaVersion === null ? "Unavailable" : `v${resultSchemaVersion}`,
      fullValue: null,
    }),
    Object.freeze({
      label: "Horizon",
      value: horizonSteps === null ? "Unavailable" : `${horizonSteps} steps`,
      fullValue: null,
    }),
    Object.freeze({
      label: "State grid",
      value: reservoirPoints === null ? "Unavailable" : `${reservoirPoints} storage points`,
      fullValue: null,
    }),
    Object.freeze({ label: "Action steps", value: actionGrid, fullValue: null }),
    hashItem("Scenario SHA-256", provenance.scenario_sha256),
    hashItem("Prices SHA-256", provenance.prices_sha256),
    hashItem("Inflows SHA-256", provenance.inflows_sha256),
    hashItem("Solver SHA-256", provenance.solver_sha256),
    hashItem("Dispatch SHA-256", provenance.dispatch_sha256, "Not produced"),
  ]);
}
