export const SCENARIO_FILE_LIMIT_BYTES = 256 * 1024;
export const SERIES_FILE_LIMIT_BYTES = 8 * 1024 * 1024;

function field(key, label, defaultValue, options = {}) {
  return Object.freeze({ key, label, defaultValue, ...options });
}

export const SCENARIO_PARAMETER_GROUPS = Object.freeze([
  Object.freeze({
    title: "Time, reservoir bounds, and initial inventory",
    fields: Object.freeze([
      field("time_step_hours", "Time step [h]", 1, { min: 0, exclusiveMin: true }),
      field("reservoir_min_volume", "Reservoir minimum [10³ m³]", 0),
      field("reservoir_max_volume", "Reservoir maximum [10³ m³]", 500),
      field("initial_reservoir_volume", "Initial reservoir [10³ m³]", 250),
    ]),
  }),
  Object.freeze({
    title: "Hydraulic actions and conversion",
    fields: Object.freeze([
      field("turbine_max_flow", "Maximum turbine flow [10³ m³/h]", 40, { min: 0 }),
      field("pump_max_flow", "Maximum pump flow [10³ m³/h]", 30, { min: 0 }),
      field("spill_max_flow", "Maximum spill flow [10³ m³/h]", 50, { min: 0 }),
      field("turbine_efficiency", "Turbine efficiency [fraction]", 0.9, { min: 0, max: 1, exclusiveMin: true }),
      field("pump_efficiency", "Pump efficiency [fraction]", 0.85, { min: 0, max: 1, exclusiveMin: true }),
      field("water_to_power_factor", "Water-to-power factor [MW/(10³ m³/h)]", 0.4, { min: 0, exclusiveMin: true }),
    ]),
  }),
  Object.freeze({
    title: "Economic parameters",
    fields: Object.freeze([
      field("operating_cost_per_mwh", "Operating cost [€/MWh]", 1, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Terminal reservoir constraints and target",
    fields: Object.freeze([
      field("terminal_reservoir_min_volume", "Terminal reservoir minimum [10³ m³]", 187.5),
      field("terminal_reservoir_max_volume", "Terminal reservoir maximum [10³ m³]", 312.5),
      field("terminal_target_reservoir_volume", "Terminal reservoir target [10³ m³]", 250),
      field("terminal_reservoir_target_penalty", "Reservoir target penalty [€/(10³ m³)²]", 20, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Solver resolution",
    note: "Higher grid and action counts can increase solve time and memory use sharply.",
    fields: Object.freeze([
      field("reservoir_volume_grid_points", "Reservoir grid points [count]", 9, { integer: true, min: 1 }),
      field("turbine_flow_steps", "Turbine flow steps [count]", 3, { integer: true, min: 1 }),
      field("spill_flow_steps", "Spill flow steps [count]", 2, { integer: true, min: 1 }),
      field("pump_flow_steps", "Pump flow steps [count]", 3, { integer: true, min: 1 }),
      field("discount_factor", "Discount factor [fraction]", 1, { min: 0, max: 1 }),
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

function parseFieldValue(definition, rawValue) {
  const text = String(rawValue ?? "").trim();
  requireCondition(text.length > 0, `${definition.label} is required.`);
  const value = Number(text);
  requireCondition(Number.isFinite(value), `${definition.label} must be a finite number.`);
  if (definition.integer) {
    requireCondition(Number.isSafeInteger(value), `${definition.label} must be an integer.`);
  }
  if (definition.min !== undefined) {
    const valid = definition.exclusiveMin ? value > definition.min : value >= definition.min;
    requireCondition(valid, `${definition.label} is below its allowed minimum.`);
  }
  if (definition.max !== undefined) {
    requireCondition(value <= definition.max, `${definition.label} exceeds its allowed maximum.`);
  }
  return { text, value };
}

function validateBounds(values) {
  requireCondition(
    values.reservoir_min_volume <= values.reservoir_max_volume,
    "Reservoir minimum volume cannot exceed its maximum.",
  );
  requireCondition(
    values.initial_reservoir_volume >= values.reservoir_min_volume
      && values.initial_reservoir_volume <= values.reservoir_max_volume,
    "Initial reservoir volume must be inside the reservoir bounds.",
  );
  requireCondition(
    values.terminal_reservoir_min_volume >= values.reservoir_min_volume
      && values.terminal_reservoir_max_volume <= values.reservoir_max_volume
      && values.terminal_reservoir_min_volume <= values.terminal_reservoir_max_volume,
    "Terminal reservoir bounds must be ordered and inside the model bounds.",
  );
  requireCondition(
    values.terminal_target_reservoir_volume >= values.terminal_reservoir_min_volume
      && values.terminal_target_reservoir_volume <= values.terminal_reservoir_max_volume,
    "Terminal reservoir target must be inside the terminal bounds.",
  );
  if (values.reservoir_min_volume < values.reservoir_max_volume) {
    requireCondition(
      values.reservoir_volume_grid_points >= 2,
      "Reservoir grid points must be at least two for a nonzero reservoir range.",
    );
  }
}

export function buildScenarioCsv(name, values) {
  const normalizedName = normalizeScenarioName(name);
  const parsedValues = {};
  const lines = ["key,value", `scenario_name,${normalizedName}`];
  for (const definition of ALL_FIELDS) {
    const parsed = parseFieldValue(definition, values[definition.key]);
    parsedValues[definition.key] = parsed.value;
    lines.push(`${definition.key},${parsed.text}`);
  }
  validateBounds(parsedValues);
  return `${lines.join("\n")}\n`;
}

const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;

function parseTimestampUtc(value, rowNumber) {
  requireCondition(
    UTC_TIMESTAMP_PATTERN.test(value),
    `Row ${rowNumber} timestamp_utc must use YYYY-MM-DDTHH:MM:SSZ.`,
  );
  const milliseconds = Date.parse(value);
  const canonical = Number.isFinite(milliseconds)
    ? new Date(milliseconds).toISOString().replace(".000Z", "Z")
    : "";
  requireCondition(canonical === value, `Row ${rowNumber} has an invalid timestamp_utc.`);
  return Object.freeze({ text: value, milliseconds });
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
    header.length === 2 && header[0] === "timestamp_utc" && header[1] === valueColumn,
    `Expected header timestamp_utc,${valueColumn}.`,
  );

  const timestamps = [];
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trim();
    if (!line) continue;
    const columns = line.split(",").map((item) => item.trim());
    const rowNumber = lineIndex + 2;
    requireCondition(columns.length === 2, `Row ${rowNumber} must contain two columns.`);
    const timestamp = parseTimestampUtc(columns[0], rowNumber);
    if (timestamps.length > 0) {
      requireCondition(
        timestamp.milliseconds > timestamps.at(-1).milliseconds,
        `Row ${rowNumber} timestamp_utc must be strictly increasing.`,
      );
    }
    const value = Number(columns[1]);
    requireCondition(Number.isFinite(value), `Row ${rowNumber} has an invalid ${valueColumn}.`);
    if (valueColumn === "natural_inflow") {
      requireCondition(value >= 0, `Row ${rowNumber} has a negative natural_inflow.`);
    }
    timestamps.push(timestamp);
  }
  requireCondition(timestamps.length > 0, `${valueColumn} CSV must contain at least one data row.`);
  return Object.freeze({ rowCount: timestamps.length, timestamps: Object.freeze(timestamps) });
}

export function validateSeriesPair(pricesText, inflowsText, timeStepHours) {
  const prices = validateSeriesCsv(pricesText, "price");
  const inflows = validateSeriesCsv(inflowsText, "natural_inflow");
  requireCondition(
    prices.rowCount === inflows.rowCount,
    `Price and inflow horizons differ: ${prices.rowCount} versus ${inflows.rowCount}.`,
  );

  const parsedTimeStepHours = Number(timeStepHours);
  requireCondition(
    Number.isFinite(parsedTimeStepHours) && parsedTimeStepHours > 0,
    "Time step must be finite and positive before validating the series.",
  );
  const intervalSeconds = parsedTimeStepHours * 3600;
  const roundedIntervalSeconds = Math.round(intervalSeconds);
  requireCondition(
    Math.abs(intervalSeconds - roundedIntervalSeconds) <= 1e-9,
    "Time step must resolve to a whole number of seconds for timestamped series.",
  );
  const intervalMilliseconds = roundedIntervalSeconds * 1000;

  for (let index = 0; index < prices.rowCount; index += 1) {
    const priceTimestamp = prices.timestamps[index];
    const inflowTimestamp = inflows.timestamps[index];
    requireCondition(
      priceTimestamp.text === inflowTimestamp.text,
      `Price and inflow timestamps differ at row ${index + 2}.`,
    );
    if (index > 0) {
      requireCondition(
        priceTimestamp.milliseconds - prices.timestamps[index - 1].milliseconds
          === intervalMilliseconds,
        `Timestamp spacing at row ${index + 2} must equal time_step_hours.`,
      );
    }
  }

  return Object.freeze({
    rowCount: prices.rowCount,
    startTimestampUtc: prices.timestamps[0].text,
    endTimestampUtc: prices.timestamps.at(-1).text,
  });
}
