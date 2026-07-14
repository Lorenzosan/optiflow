export const SCENARIO_FILE_LIMIT_BYTES = 256 * 1024;
export const SERIES_FILE_LIMIT_BYTES = 8 * 1024 * 1024;

function field(key, label, defaultValue, options = {}) {
  return Object.freeze({ key, label, defaultValue, ...options });
}

export const SCENARIO_PARAMETER_GROUPS = Object.freeze([
  Object.freeze({
    title: "Time and hydraulic storage content",
    fields: Object.freeze([
      field("time_step_hours", "Time step [h]", 1, { min: 0, exclusiveMin: true }),
      field("reservoir_min_volume", "Minimum storage content [MWh hydraulic]", 0),
      field("reservoir_max_volume", "Maximum storage content [MWh hydraulic]", 200),
      field("initial_reservoir_volume", "Initial storage content [MWh hydraulic]", 100),
    ]),
  }),
  Object.freeze({
    title: "Hydraulic power limits and efficiencies",
    note: "Efficiencies are entered as percentages; the generated optimizer CSV stores fractions between 0 and 1.",
    fields: Object.freeze([
      field("turbine_max_flow", "Maximum turbine withdrawal [MW hydraulic]", 16, { min: 0 }),
      field("pump_max_flow", "Maximum pump addition [MW hydraulic]", 12, { min: 0 }),
      field("spill_max_flow", "Maximum spill [MW hydraulic]", 20, { min: 0 }),
      field("turbine_efficiency", "Turbine efficiency [%]", 90, {
        min: 0,
        max: 100,
        exclusiveMin: true,
        scenarioScale: 0.01,
        step: 0.1,
        help: "Electrical generation divided by hydraulic turbine withdrawal.",
      }),
      field("pump_efficiency", "Pump efficiency [%]", 85, {
        min: 0,
        max: 100,
        exclusiveMin: true,
        scenarioScale: 0.01,
        step: 0.1,
        help: "Hydraulic storage addition divided by electrical pumping consumption.",
      }),
    ]),
  }),
  Object.freeze({
    title: "Economic parameters",
    fields: Object.freeze([
      field("operating_cost_per_mwh", "Operating cost [€/MWh]", 1, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Terminal hydraulic storage constraints and target",
    fields: Object.freeze([
      field("terminal_reservoir_min_volume", "Terminal minimum content [MWh hydraulic]", 75),
      field("terminal_reservoir_max_volume", "Terminal maximum content [MWh hydraulic]", 125),
      field("terminal_target_reservoir_volume", "Terminal target content [MWh hydraulic]", 100),
      field("terminal_reservoir_target_penalty", "Storage target penalty [€/MWh²]", 125, { min: 0 }),
    ]),
  }),
  Object.freeze({
    title: "Solver resolution",
    note: "More points and steps refine the discrete approximation but increase runtime and memory. Compare genuinely nested resolutions before treating a result as numerically stable.",
    fields: Object.freeze([
      field("reservoir_volume_grid_points", "Storage grid points [count]", 9, { integer: true, min: 1 }),
      field("turbine_flow_steps", "Turbine withdrawal steps [count]", 3, { integer: true, min: 1 }),
      field("spill_flow_steps", "Spill steps [count]", 2, { integer: true, min: 1 }),
      field("pump_flow_steps", "Pump addition steps [count]", 3, { integer: true, min: 1 }),
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
  const scenarioValue = definition.scenarioScale === undefined
    ? value
    : value * definition.scenarioScale;
  const scenarioText = Number.isInteger(scenarioValue)
    ? String(scenarioValue)
    : String(Number(scenarioValue.toPrecision(15)));
  return { value, scenarioText };
}

function validateBounds(values) {
  requireCondition(
    values.reservoir_min_volume <= values.reservoir_max_volume,
    "Minimum storage content cannot exceed maximum storage content.",
  );
  requireCondition(
    values.initial_reservoir_volume >= values.reservoir_min_volume
      && values.initial_reservoir_volume <= values.reservoir_max_volume,
    "Initial storage content must be inside the storage bounds.",
  );
  requireCondition(
    values.terminal_reservoir_min_volume >= values.reservoir_min_volume
      && values.terminal_reservoir_max_volume <= values.reservoir_max_volume
      && values.terminal_reservoir_min_volume <= values.terminal_reservoir_max_volume,
    "Terminal storage bounds must be ordered and inside the model bounds.",
  );
  requireCondition(
    values.terminal_target_reservoir_volume >= values.terminal_reservoir_min_volume
      && values.terminal_target_reservoir_volume <= values.terminal_reservoir_max_volume,
    "Terminal storage target must be inside the terminal bounds.",
  );
  if (values.reservoir_min_volume < values.reservoir_max_volume) {
    requireCondition(
      values.reservoir_volume_grid_points >= 2,
      "Storage grid points must be at least two for a nonzero storage range.",
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
    lines.push(`${definition.key},${parsed.scenarioText}`);
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
