export const SCENARIO_FILE_LIMIT_BYTES = 256 * 1024;
export const SERIES_FILE_LIMIT_BYTES = 8 * 1024 * 1024;

function field(key, label, defaultValue, options = {}) {
  return Object.freeze({ key, label, defaultValue, ...options });
}

export const SCENARIO_PARAMETER_GROUPS = Object.freeze([
  Object.freeze({
    title: "Time, storage bounds, and initial state",
    fields: Object.freeze([
      field("time_step_hours", "Time step (hours)", 1, { min: 0, exclusiveMin: true }),
      field("reservoir_min_volume", "Reservoir minimum volume", 0),
      field("reservoir_max_volume", "Reservoir maximum volume", 500),
      field("initial_reservoir_volume", "Initial reservoir volume", 250),
      field("battery_min_soc", "Battery minimum SOC", 0),
      field("battery_max_soc", "Battery maximum SOC", 100),
      field("initial_battery_soc", "Initial battery SOC", 50),
    ]),
  }),
  Object.freeze({
    title: "Actions and conversion",
    fields: Object.freeze([
      field("turbine_max_flow", "Maximum turbine flow", 40, { min: 0 }),
      field("pump_max_flow", "Maximum pump flow", 30, { min: 0 }),
      field("spill_max_flow", "Maximum spill flow", 50, { min: 0 }),
      field("battery_max_charge_power", "Maximum battery charge power", 25, { min: 0 }),
      field("battery_max_discharge_power", "Maximum battery discharge power", 25, { min: 0 }),
      field("turbine_efficiency", "Turbine efficiency", 0.9, { min: 0, max: 1, exclusiveMin: true }),
      field("pump_efficiency", "Pump efficiency", 0.85, { min: 0, max: 1, exclusiveMin: true }),
      field("battery_charge_efficiency", "Battery charge efficiency", 1, { min: 0, max: 1, exclusiveMin: true }),
      field("battery_discharge_efficiency", "Battery discharge efficiency", 1, { min: 0, max: 1, exclusiveMin: true }),
      field("water_to_power_factor", "Water-to-power factor", 0.4, { min: 0, exclusiveMin: true }),
    ]),
  }),
  Object.freeze({
    title: "Economic parameters",
    fields: Object.freeze([
      field("battery_degradation_cost_per_mwh", "Battery degradation cost / MWh", 3, { min: 0 }),
      field("operating_cost_per_mwh", "Operating cost / MWh", 1, { min: 0 }),
      field("infeasibility_penalty", "Infeasibility penalty", 1000000, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Terminal constraints and targets",
    fields: Object.freeze([
      field("terminal_reservoir_min_volume", "Terminal reservoir minimum", 187.5),
      field("terminal_reservoir_max_volume", "Terminal reservoir maximum", 312.5),
      field("terminal_target_reservoir_volume", "Terminal reservoir target", 250),
      field("terminal_reservoir_target_penalty", "Reservoir target penalty", 20, { min: 0 }),
      field("terminal_battery_min_soc", "Terminal battery minimum SOC", 25),
      field("terminal_battery_max_soc", "Terminal battery maximum SOC", 75),
      field("terminal_target_battery_soc", "Terminal battery target SOC", 50),
      field("terminal_battery_target_penalty", "Battery target penalty", 20, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Solver resolution",
    note: "Higher grid and action counts can increase solve time and memory use sharply.",
    fields: Object.freeze([
      field("reservoir_volume_grid_points", "Reservoir grid points", 9, { integer: true, min: 1 }),
      field("battery_soc_grid_points", "Battery SOC grid points", 5, { integer: true, min: 1 }),
      field("turbine_flow_steps", "Turbine flow steps", 3, { integer: true, min: 1 }),
      field("spill_flow_steps", "Spill flow steps", 2, { integer: true, min: 1 }),
      field("pump_flow_steps", "Pump flow steps", 3, { integer: true, min: 1 }),
      field("battery_charge_steps", "Battery charge steps", 2, { integer: true, min: 1 }),
      field("battery_discharge_steps", "Battery discharge steps", 2, { integer: true, min: 1 }),
      field("discount_factor", "Discount factor", 1, { min: 0, max: 1 }),
    ]),
  }),
]);

const ALL_FIELDS = SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields);

function requireCondition(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function normalizeScenarioName(name) {
  const normalized = String(name ?? "").trim();
  requireCondition(normalized.length > 0, "Scenario name is required.");
  requireCondition(normalized.length <= 128, "Scenario name must be at most 128 characters.");
  requireCondition(!/[\r\n,]/.test(normalized), "Scenario name cannot contain commas or line breaks.");
  return normalized;
}

function parseFieldValue(fieldDefinition, rawValue) {
  const text = String(rawValue ?? "").trim();
  requireCondition(text.length > 0, `${fieldDefinition.label} is required.`);
  const value = Number(text);
  requireCondition(Number.isFinite(value), `${fieldDefinition.label} must be a finite number.`);
  if (fieldDefinition.integer) {
    requireCondition(Number.isSafeInteger(value), `${fieldDefinition.label} must be an integer.`);
  }
  if (fieldDefinition.min !== undefined) {
    const valid = fieldDefinition.exclusiveMin
      ? value > fieldDefinition.min
      : value >= fieldDefinition.min;
    requireCondition(valid, `${fieldDefinition.label} is below its allowed minimum.`);
  }
  if (fieldDefinition.max !== undefined) {
    requireCondition(value <= fieldDefinition.max, `${fieldDefinition.label} exceeds its allowed maximum.`);
  }
  return { text, value };
}

function validateBounds(values) {
  requireCondition(
    values.reservoir_min_volume <= values.reservoir_max_volume,
    "Reservoir minimum volume cannot exceed its maximum.",
  );
  requireCondition(
    values.battery_min_soc <= values.battery_max_soc,
    "Battery minimum SOC cannot exceed its maximum.",
  );
  requireCondition(
    values.initial_reservoir_volume >= values.reservoir_min_volume
      && values.initial_reservoir_volume <= values.reservoir_max_volume,
    "Initial reservoir volume must be inside the reservoir bounds.",
  );
  requireCondition(
    values.initial_battery_soc >= values.battery_min_soc
      && values.initial_battery_soc <= values.battery_max_soc,
    "Initial battery SOC must be inside the battery bounds.",
  );

  requireCondition(
    values.terminal_reservoir_min_volume >= values.reservoir_min_volume
      && values.terminal_reservoir_max_volume <= values.reservoir_max_volume
      && values.terminal_reservoir_min_volume <= values.terminal_reservoir_max_volume,
    "Terminal reservoir bounds must be ordered and inside the model bounds.",
  );
  requireCondition(
    values.terminal_battery_min_soc >= values.battery_min_soc
      && values.terminal_battery_max_soc <= values.battery_max_soc
      && values.terminal_battery_min_soc <= values.terminal_battery_max_soc,
    "Terminal battery bounds must be ordered and inside the model bounds.",
  );
  requireCondition(
    values.terminal_target_reservoir_volume >= values.terminal_reservoir_min_volume
      && values.terminal_target_reservoir_volume <= values.terminal_reservoir_max_volume,
    "Terminal reservoir target must be inside the terminal bounds.",
  );
  requireCondition(
    values.terminal_target_battery_soc >= values.terminal_battery_min_soc
      && values.terminal_target_battery_soc <= values.terminal_battery_max_soc,
    "Terminal battery target must be inside the terminal bounds.",
  );

  if (values.reservoir_min_volume < values.reservoir_max_volume) {
    requireCondition(
      values.reservoir_volume_grid_points >= 2,
      "Reservoir grid points must be at least two for a nonzero reservoir range.",
    );
  }
  if (values.battery_min_soc < values.battery_max_soc) {
    requireCondition(
      values.battery_soc_grid_points >= 2,
      "Battery grid points must be at least two for a nonzero battery range.",
    );
  }
}

export function buildScenarioCsv(name, values) {
  const normalizedName = normalizeScenarioName(name);
  const parsedValues = {};
  const lines = ["key,value", `scenario_name,${normalizedName}`];

  for (const fieldDefinition of ALL_FIELDS) {
    const parsed = parseFieldValue(fieldDefinition, values[fieldDefinition.key]);
    parsedValues[fieldDefinition.key] = parsed.value;
    lines.push(`${fieldDefinition.key},${parsed.text}`);
  }

  validateBounds(parsedValues);
  return `${lines.join("\n")}\n`;
}

export function validateSeriesCsv(text, valueColumn) {
  requireCondition(
    valueColumn === "price" || valueColumn === "natural_inflow",
    "Unsupported series column.",
  );
  requireCondition(!String(text).includes("\uFFFD"), `${valueColumn} CSV is not valid UTF-8.`);

  const lines = String(text).split(/\r?\n/);
  const header = (lines.shift() ?? "").split(",").map((item) => item.trim());
  requireCondition(
    header.length === 2 && header[0] === "time_index" && header[1] === valueColumn,
    `Expected header time_index,${valueColumn}.`,
  );

  let expectedIndex = 0;
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trim();
    if (!line) {
      continue;
    }
    const columns = line.split(",").map((item) => item.trim());
    requireCondition(columns.length === 2, `Row ${lineIndex + 2} must contain two columns.`);
    requireCondition(/^\d+$/.test(columns[0]), `Row ${lineIndex + 2} has an invalid time_index.`);
    const timeIndex = Number(columns[0]);
    requireCondition(
      Number.isSafeInteger(timeIndex) && timeIndex === expectedIndex,
      `Row ${lineIndex + 2} must use time_index ${expectedIndex}.`,
    );
    const value = Number(columns[1]);
    requireCondition(Number.isFinite(value), `Row ${lineIndex + 2} has an invalid ${valueColumn}.`);
    if (valueColumn === "natural_inflow") {
      requireCondition(value >= 0, `Row ${lineIndex + 2} has a negative natural_inflow.`);
    }
    expectedIndex += 1;
  }

  requireCondition(expectedIndex > 0, `${valueColumn} CSV must contain at least one data row.`);
  return Object.freeze({ rowCount: expectedIndex });
}

export function validateSeriesPair(pricesText, inflowsText) {
  const prices = validateSeriesCsv(pricesText, "price");
  const inflows = validateSeriesCsv(inflowsText, "natural_inflow");
  requireCondition(
    prices.rowCount === inflows.rowCount,
    `Price and inflow horizons differ: ${prices.rowCount} versus ${inflows.rowCount}.`,
  );
  return Object.freeze({ rowCount: prices.rowCount });
}
