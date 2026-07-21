/**
 * @file
 * @brief Scalar scenario serialization and timestamped-series validation.
 *
 * The editor presents user-friendly units and interval counts while this module
 * converts them to the exact CSV schema consumed by the C++ optimizer.
 */

/**
 * @brief Maximum accepted scalar scenario file size in bytes.
 */
export const SCENARIO_FILE_LIMIT_BYTES = 256 * 1024;
/**
 * @brief Maximum accepted price or inflow series file size in bytes.
 */
export const SERIES_FILE_LIMIT_BYTES = 8 * 1024 * 1024;

/**
 * @brief Creates an immutable scenario editor field definition.
 * @param key Optimizer CSV key.
 * @param label Visible editor label.
 * @param defaultValue Initial editor value.
 * @param options Validation and conversion metadata.
 * @return A frozen field definition.
 */
function field(key, label, defaultValue, options = {}) {
  return Object.freeze({ key, label, defaultValue, ...options });
}

/**
 * @brief Immutable grouped field definitions used to build the scalar editor.
 */
export const SCENARIO_PARAMETER_GROUPS = Object.freeze([
  Object.freeze({
    title: "Hydraulic storage content",
    fields: Object.freeze([
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
    note: "More intervals and action steps refine the discrete approximation but increase runtime and memory. Compare genuinely nested resolutions before treating a result as numerically stable.",
    fields: Object.freeze([
      field("reservoir_volume_grid_points", "Storage grid intervals [count]", 8, {
        integer: true,
        min: 0,
        scenarioOffset: 1,
        help: "The optimizer uses intervals + 1 grid points. Spacing equals storage range divided by intervals.",
      }),
      field("turbine_flow_steps", "Turbine withdrawal steps [count]", 3, { integer: true, min: 1 }),
      field("spill_flow_steps", "Spill steps [count]", 2, { integer: true, min: 1 }),
      field("pump_flow_steps", "Pump addition steps [count]", 3, { integer: true, min: 1 }),
      field("discount_factor", "Discount factor [fraction]", 1, { min: 0, max: 1 }),
    ]),
  }),
]);

/**
 * @brief Flattened field definitions used for parsing and serialization.
 */
const ALL_FIELDS = SCENARIO_PARAMETER_GROUPS.flatMap((group) => group.fields);

/**
 * @brief Throws a validation error when a required condition is false.
 * @param condition Condition to enforce.
 * @param message Error message.
 * @throws Error when the condition is false.
 */
function requireCondition(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

/**
 * @brief Trims and validates a scenario name for the two-column CSV format.
 * @param name Candidate scenario name.
 * @return The normalized name.
 * @throws Error when the name is empty, too long, or CSV-unsafe.
 */
function normalizeScenarioName(name) {
  const normalized = String(name ?? "").trim();
  requireCondition(normalized.length > 0, "Scenario name is required.");
  requireCondition(normalized.length <= 128, "Scenario name must be at most 128 characters.");
  requireCondition(!/[\r\n,]/.test(normalized), "Scenario name cannot contain commas or line breaks.");
  return normalized;
}

/**
 * @brief Serializes a finite number without unnecessary trailing precision.
 * @param value Numeric value.
 * @return Stable decimal text suitable for CSV output.
 */
function numberText(value) {
  return Number.isInteger(value) ? String(value) : String(Number(value.toPrecision(15)));
}

/**
 * @brief Validates an editor value and converts it to optimizer units.
 * @param definition Field conversion and validation metadata.
 * @param rawValue Value supplied by the editor or parsed scenario.
 * @return The editor value and serialized optimizer value.
 * @throws Error when validation fails.
 */
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
  const offsetValue = value + (definition.scenarioOffset ?? 0);
  const scenarioValue = definition.scenarioScale === undefined
    ? offsetValue
    : offsetValue * definition.scenarioScale;
  return { value, scenarioText: numberText(scenarioValue) };
}

/**
 * @brief Validates storage, terminal, and grid relationships across fields.
 * @param values Validated editor values keyed by scenario parameter.
 * @throws Error when related bounds or resolutions are inconsistent.
 */
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
      values.reservoir_volume_grid_points >= 1,
      "Storage grid intervals must be at least one for a nonzero storage range.",
    );
  }
}

/**
 * @brief Validates a positive time step representable as whole seconds.
 * @param value Candidate duration in hours.
 * @return The numeric duration in hours.
 * @throws Error when the duration is invalid.
 */
function validateTimeStepHours(value) {
  const parsed = Number(value);
  requireCondition(Number.isFinite(parsed) && parsed > 0, "Derived time step must be finite and positive.");
  const seconds = parsed * 3600;
  requireCondition(
    Math.abs(seconds - Math.round(seconds)) <= 1e-9,
    "Derived time step must resolve to a whole number of seconds.",
  );
  return parsed;
}

/**
 * @brief Builds a complete optimizer `key,value` scenario CSV.
 * @param name Scenario name.
 * @param values Editor values keyed by parameter name.
 * @param timeStepHours Duration derived from series timestamps.
 * @return UTF-8 CSV text ending with a newline.
 * @throws Error when any scalar value is invalid.
 */
export function buildScenarioCsv(name, values, timeStepHours) {
  const normalizedName = normalizeScenarioName(name);
  const parsedTimeStepHours = validateTimeStepHours(timeStepHours);
  const parsedValues = {};
  const lines = [
    "key,value",
    `scenario_name,${normalizedName}`,
    `time_step_hours,${numberText(parsedTimeStepHours)}`,
  ];
  for (const definition of ALL_FIELDS) {
    const parsed = parseFieldValue(definition, values[definition.key]);
    parsedValues[definition.key] = parsed.value;
    lines.push(`${definition.key},${parsed.scenarioText}`);
  }
  validateBounds(parsedValues);
  return `${lines.join("\n")}\n`;
}

/**
 * @brief Parses and validates an optimizer scenario CSV for editor hydration.
 * @param text Scenario CSV text.
 * @return Normalized scenario name, time step, and editor values.
 * @throws Error when the schema, keys, or values are invalid.
 */
export function parseScenarioCsv(text) {
  requireCondition(!String(text).includes("\uFFFD"), "Scenario CSV is not valid UTF-8.");
  const lines = String(text).split(/\r?\n/);
  const header = (lines.shift() ?? "").split(",").map((item) => item.trim());
  requireCondition(
    header.length === 2 && header[0] === "key" && header[1] === "value",
    "Expected scenario header key,value.",
  );

  const supportedKeys = new Set([
    "scenario_name",
    "time_step_hours",
    ...ALL_FIELDS.map((definition) => definition.key),
  ]);
  const parameters = new Map();
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trim();
    if (!line) continue;
    const columns = line.split(",").map((item) => item.trim());
    const rowNumber = lineIndex + 2;
    requireCondition(columns.length === 2, `Scenario row ${rowNumber} must contain two columns.`);
    const [key, value] = columns;
    requireCondition(key.length > 0, `Scenario row ${rowNumber} has an empty key.`);
    requireCondition(supportedKeys.has(key), `Scenario row ${rowNumber} has unsupported key ${key}.`);
    requireCondition(!parameters.has(key), `Scenario row ${rowNumber} duplicates key ${key}.`);
    parameters.set(key, value);
  }

  const missing = [...supportedKeys].filter((key) => !parameters.has(key));
  requireCondition(missing.length === 0, `Scenario CSV is missing: ${missing.join(", ")}.`);

  const name = normalizeScenarioName(parameters.get("scenario_name"));
  const timeStepHours = validateTimeStepHours(parameters.get("time_step_hours"));
  const values = {};
  const validationValues = {};
  for (const definition of ALL_FIELDS) {
    const scenarioValue = Number(parameters.get(definition.key));
    requireCondition(
      Number.isFinite(scenarioValue),
      `${definition.label} must be a finite number in the scenario CSV.`,
    );
    const scaledEditorValue = definition.scenarioScale === undefined
      ? scenarioValue
      : scenarioValue / definition.scenarioScale;
    const editorValue = scaledEditorValue - (definition.scenarioOffset ?? 0);
    const parsed = parseFieldValue(definition, editorValue);
    values[definition.key] = parsed.value;
    validationValues[definition.key] = parsed.value;
  }
  validateBounds(validationValues);

  return Object.freeze({
    name,
    timeStepHours,
    values: Object.freeze(values),
  });
}

/**
 * @brief Builds a unique copy name for a read-only bundled scenario.
 * @param name Original scenario name.
 * @param existingNames Names already present in the backend.
 * @return A unique name with a copy suffix.
 */
export function suggestScenarioCopyName(name, existingNames) {
  const sourceName = normalizeScenarioName(name);
  const occupied = new Set([...existingNames].map((value) => String(value)));
  for (let copyNumber = 1; copyNumber < 10_000; copyNumber += 1) {
    const suffix = copyNumber === 1 ? "_copy" : `_copy_${copyNumber}`;
    const candidate = `${sourceName.slice(0, 128 - suffix.length)}${suffix}`;
    if (!occupied.has(candidate)) {
      return candidate;
    }
  }
  throw new Error("Could not create a unique scenario copy name.");
}


const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;

/**
 * @brief Parses one canonical UTC timestamp from a series row.
 * @param value Timestamp text.
 * @param rowNumber One-based CSV row number used in errors.
 * @return Epoch milliseconds.
 * @throws Error when the timestamp is noncanonical or invalid.
 */
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

/**
 * @brief Validates one timestamped numeric series CSV.
 * @param text CSV text.
 * @param valueColumn Required numeric column name.
 * @return Canonical timestamps, numeric values, and spacing metadata.
 * @throws Error when headers, rows, timestamps, values, or spacing are invalid.
 */
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
  let minimumValue = Number.POSITIVE_INFINITY;
  let maximumValue = Number.NEGATIVE_INFINITY;
  let minimumAbsoluteValue = Number.POSITIVE_INFINITY;
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
    minimumValue = Math.min(minimumValue, value);
    maximumValue = Math.max(maximumValue, value);
    minimumAbsoluteValue = Math.min(minimumAbsoluteValue, Math.abs(value));
  }
  requireCondition(timestamps.length > 0, `${valueColumn} CSV must contain at least one data row.`);
  return Object.freeze({
    rowCount: timestamps.length,
    timestamps: Object.freeze(timestamps),
    minimumValue,
    maximumValue,
    minimumAbsoluteValue,
  });
}

/**
 * @brief Validates matching price and inflow series and derives the time step.
 * @param pricesText Price CSV text.
 * @param inflowsText Natural inflow CSV text.
 * @return Row count and time step in hours.
 * @throws Error when horizons or timestamps differ.
 */
export function validateSeriesPair(pricesText, inflowsText) {
  const prices = validateSeriesCsv(pricesText, "price");
  const inflows = validateSeriesCsv(inflowsText, "natural_inflow");
  requireCondition(
    prices.rowCount === inflows.rowCount,
    `Price and inflow horizons differ: ${prices.rowCount} versus ${inflows.rowCount}.`,
  );
  requireCondition(
    prices.rowCount >= 2,
    "At least two timestamped rows are required to derive the time step.",
  );

  const intervalMilliseconds = prices.timestamps[1].milliseconds - prices.timestamps[0].milliseconds;
  requireCondition(intervalMilliseconds > 0, "Timestamp spacing must be positive.");

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
        `Timestamp spacing is not constant at row ${index + 2}.`,
      );
    }
  }

  const timeStepHours = validateTimeStepHours(intervalMilliseconds / 3_600_000);
  return Object.freeze({
    rowCount: prices.rowCount,
    startTimestampUtc: prices.timestamps[0].text,
    endTimestampUtc: prices.timestamps.at(-1).text,
    timeStepHours,
    minimumPrice: prices.minimumValue,
    maximumPrice: prices.maximumValue,
    minimumAbsolutePrice: prices.minimumAbsoluteValue,
  });
}
