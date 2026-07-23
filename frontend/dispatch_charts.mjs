/**
 * @file
 * @brief Parsing, modeling, navigation, and SVG rendering for dispatch charts.
 *
 * The renderer is dependency-free. It maintains one synchronized time domain for
 * all panels and keeps interval-valued series as steps while storage is drawn as a
 * state trajectory including the terminal endpoint.
 */

import { formatNumber } from "./number_format.mjs";

/** Milliseconds per hour used for interval and axis calculations. */
const HOUR_MILLISECONDS = 3_600_000;
/** Namespace URI required when creating SVG DOM elements. */
const SVG_NAMESPACE = "http://www.w3.org/2000/svg";
/** Canonical whole-second UTC timestamp accepted from dispatch artifacts. */
const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;
/** Sequence used to keep generated SVG clip-path identifiers unique. */
let chartRenderSequence = 0;

/**
 * @brief Civil time zone used for chart axes, ranges, and tooltips.
 */
export const DISPATCH_CHART_TIME_ZONE = "Europe/Zurich";

/**
 * @brief Dispatch artifact columns required to construct every chart panel.
 */
const REQUIRED_COLUMNS = Object.freeze([
  "time_index",
  "timestamp_utc",
  "price",
  "natural_inflow",
  "reservoir_volume",
  "turbine_flow",
  "spill_flow",
  "pump_flow",
  "next_reservoir_volume",
  "net_power",
  "reward",
]);

/**
 * @brief Throws a chart-data error when a required condition is false.
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
 * @brief Parses a finite numeric dispatch cell.
 * @param value CSV cell text.
 * @param label Column name used in errors.
 * @param rowNumber One-based CSV row number.
 * @return The parsed number.
 * @throws Error when the value is not finite.
 */
function parseFinite(value, label, rowNumber) {
  const parsed = Number(value);
  requireCondition(Number.isFinite(parsed), `Dispatch row ${rowNumber} has an invalid ${label}.`);
  return parsed;
}

/**
 * @brief Parses one canonical UTC dispatch timestamp.
 * @param value Timestamp text.
 * @param rowNumber One-based CSV row number.
 * @return Epoch milliseconds.
 * @throws Error when the timestamp is invalid.
 */
function parseTimestampUtc(value, rowNumber) {
  requireCondition(
    UTC_TIMESTAMP_PATTERN.test(value),
    `Dispatch row ${rowNumber} timestamp_utc must use YYYY-MM-DDTHH:MM:SSZ.`,
  );
  const milliseconds = Date.parse(value);
  const canonical = Number.isFinite(milliseconds)
    ? new Date(milliseconds).toISOString().replace(".000Z", "Z")
    : "";
  requireCondition(canonical === value, `Dispatch row ${rowNumber} has an invalid timestamp_utc.`);
  return milliseconds;
}

/**
 * @brief Converts a positive whole-second time step from hours to milliseconds.
 * @param timeStepHours Scenario interval duration in hours.
 * @return Interval duration in milliseconds.
 * @throws Error when the duration is invalid.
 */
function intervalMilliseconds(timeStepHours) {
  const parsed = Number(timeStepHours);
  requireCondition(Number.isFinite(parsed) && parsed > 0, "Scenario time step is invalid.");
  const seconds = parsed * 3600;
  const roundedSeconds = Math.round(seconds);
  requireCondition(
    Math.abs(seconds - roundedSeconds) <= 1e-9,
    "Scenario time step must resolve to a whole number of seconds.",
  );
  return roundedSeconds * 1000;
}

/**
 * @brief Parses and validates the dispatch fields required by the chart model.
 * @param text Dispatch CSV text.
 * @param timeStepHours Scenario interval duration in hours.
 * @return Immutable dispatch rows and interval duration.
 * @throws Error when headers, rows, or spacing are invalid.
 */
function parseDispatchRows(text, timeStepHours) {
  const lines = String(text).split(/\r?\n/);
  const header = (lines.shift() ?? "").split(",").map((item) => item.trim());
  const positions = Object.fromEntries(header.map((name, index) => [name, index]));
  for (const required of REQUIRED_COLUMNS) {
    requireCondition(required in positions, `Dispatch CSV is missing ${required}.`);
  }

  const stepMilliseconds = intervalMilliseconds(timeStepHours);
  const rows = [];
  let expectedIndex = 0;
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trim();
    if (!line) {
      continue;
    }
    const columns = line.split(",").map((item) => item.trim());
    const rowNumber = lineIndex + 2;
    requireCondition(
      columns.length === header.length,
      `Dispatch row ${rowNumber} has the wrong column count.`,
    );
    const timeIndex = Number(columns[positions.time_index]);
    requireCondition(
      Number.isSafeInteger(timeIndex) && timeIndex === expectedIndex,
      `Dispatch row ${rowNumber} must use time_index ${expectedIndex}.`,
    );
    const timestampMilliseconds = parseTimestampUtc(
      columns[positions.timestamp_utc],
      rowNumber,
    );
    if (rows.length > 0) {
      requireCondition(
        timestampMilliseconds - rows.at(-1).timestampMilliseconds === stepMilliseconds,
        `Dispatch timestamp spacing at row ${rowNumber} does not match time_step_hours.`,
      );
    }
    const price = parseFinite(columns[positions.price], "price", rowNumber);
    const netPower = parseFinite(columns[positions.net_power], "net_power", rowNumber);
    const netOperatingCashflow = parseFinite(columns[positions.reward], "reward", rowNumber);
    const intervalHours = stepMilliseconds / HOUR_MILLISECONDS;
    const marketCashflow = price * netPower * intervalHours;
    // The solver artifact stores net reward; recover modeled cost for its own panel.
    const rawOperatingCost = marketCashflow - netOperatingCashflow;
    const operatingCost = Math.abs(rawOperatingCost) <= 1e-9 ? 0 : rawOperatingCost;
    rows.push(Object.freeze({
      timeIndex,
      timestampUtc: columns[positions.timestamp_utc],
      timestampMilliseconds,
      price,
      naturalInflow: parseFinite(
        columns[positions.natural_inflow],
        "natural_inflow",
        rowNumber,
      ),
      reservoirVolume: parseFinite(
        columns[positions.reservoir_volume],
        "reservoir_volume",
        rowNumber,
      ),
      turbineFlow: parseFinite(columns[positions.turbine_flow], "turbine_flow", rowNumber),
      spillFlow: parseFinite(columns[positions.spill_flow], "spill_flow", rowNumber),
      pumpFlow: parseFinite(columns[positions.pump_flow], "pump_flow", rowNumber),
      nextReservoirVolume: parseFinite(
        columns[positions.next_reservoir_volume],
        "next_reservoir_volume",
        rowNumber,
      ),
      netPower,
      marketCashflow,
      operatingCost,
      netOperatingCashflow,
    }));
    expectedIndex += 1;
  }
  requireCondition(rows.length > 0, "Dispatch CSV contains no data rows.");
  return { rows: Object.freeze(rows), stepMilliseconds };
}

/**
 * @brief Builds points for an interval-valued step series including its right endpoint.
 * @param rows Parsed dispatch rows.
 * @param valueForRow Selector returning the plotted value.
 * @param stepMilliseconds Interval duration in milliseconds.
 * @return An immutable point sequence.
 */
function stepPoints(rows, valueForRow, stepMilliseconds) {
  const points = rows.map((row) => Object.freeze({
    timestampMilliseconds: row.timestampMilliseconds,
    value: valueForRow(row),
  }));
  points.push(Object.freeze({
    timestampMilliseconds: rows.at(-1).timestampMilliseconds + stepMilliseconds,
    value: valueForRow(rows.at(-1)),
  }));
  return Object.freeze(points);
}

/**
 * @brief Builds points for a state-valued line series.
 * @param rows Parsed dispatch rows.
 * @param valueForRow Selector returning the plotted value.
 * @return An immutable point sequence.
 */
function linePoints(rows, valueForRow) {
  return Object.freeze(rows.map((row) => Object.freeze({
    timestampMilliseconds: row.timestampMilliseconds,
    value: valueForRow(row),
  })));
}

/**
 * @brief Derives a human-readable operating mode for the tooltip.
 * @param row Parsed dispatch row.
 * @return Idle or a combined generating, pumping, and spilling label.
 */
function operatingMode(row) {
  const modes = [];
  if (row.turbineFlow > 0) {
    modes.push("generating");
  }
  if (row.pumpFlow > 0) {
    modes.push("pumping");
  }
  if (row.spillFlow > 0) {
    modes.push("spilling");
  }
  if (modes.length === 0) {
    return "Idle";
  }
  return modes.map((mode, index) => index === 0
    ? `${mode[0].toUpperCase()}${mode.slice(1)}`
    : mode).join(" + ");
}

/**
 * @brief Creates an immutable chart-series descriptor.
 * @param key Stable series identifier.
 * @param label Legend and tooltip label.
 * @param points Timestamped values.
 * @param className CSS class controlling line appearance.
 * @return A frozen series descriptor.
 */
function series(key, label, points, className) {
  return Object.freeze({ key, label, points, className });
}

/**
 * @brief Builds the complete immutable multi-panel chart model.
 * @param dispatchText Dispatch CSV text.
 * @param timeStepHours Scenario interval duration in hours.
 * @return Parsed rows, global time bounds, and panel descriptors.
 * @throws Error when the artifact is invalid.
 */
export function buildDispatchChartModel(dispatchText, timeStepHours) {
  const { rows, stepMilliseconds } = parseDispatchRows(dispatchText, timeStepHours);
  const startMilliseconds = rows[0].timestampMilliseconds;
  const endMilliseconds = rows.at(-1).timestampMilliseconds + stepMilliseconds;

  const reservoirPoints = linePoints(rows, (row) => row.reservoirVolume);
  const reservoirWithEnd = Object.freeze([
    ...reservoirPoints,
    Object.freeze({
      timestampMilliseconds: endMilliseconds,
      value: rows.at(-1).nextReservoirVolume,
    }),
  ]);
  return Object.freeze({
    rows,
    stepMilliseconds,
    startMilliseconds,
    endMilliseconds,
    panels: Object.freeze([
      Object.freeze({
        key: "price",
        title: "Electricity price",
        unit: "€/MWh",
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "price",
            "Price",
            stepPoints(rows, (row) => row.price, stepMilliseconds),
            "dispatch-series-price",
          ),
        ]),
      }),
      Object.freeze({
        key: "inflow",
        title: "Natural inflow",
        unit: "MW hydraulic",
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "inflow",
            "Inflow",
            stepPoints(rows, (row) => row.naturalInflow, stepMilliseconds),
            "dispatch-series-inflow",
          ),
        ]),
      }),
      Object.freeze({
        key: "turbine",
        title: "Turbine withdrawal",
        unit: "MW hydraulic",
        height: 72,
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "turbine",
            "Turbine",
            stepPoints(rows, (row) => row.turbineFlow, stepMilliseconds),
            "dispatch-series-turbine",
          ),
        ]),
      }),
      Object.freeze({
        key: "pump",
        title: "Pump addition",
        unit: "MW hydraulic",
        height: 72,
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "pump",
            "Pump",
            stepPoints(rows, (row) => row.pumpFlow, stepMilliseconds),
            "dispatch-series-pump",
          ),
        ]),
      }),
      Object.freeze({
        key: "spill",
        title: "Spill",
        unit: "MW hydraulic",
        height: 72,
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "spill",
            "Spill",
            stepPoints(rows, (row) => row.spillFlow, stepMilliseconds),
            "dispatch-series-spill",
          ),
        ]),
      }),
      Object.freeze({
        key: "reservoir",
        title: "Storage content",
        unit: "MWh hydraulic",
        interpolation: "line",
        includeZero: true,
        series: Object.freeze([
          series(
            "reservoir",
            "Storage content",
            reservoirWithEnd,
            "dispatch-series-reservoir",
          ),
        ]),
      }),
      Object.freeze({
        key: "cashflow",
        title: "Net operating cashflow",
        unit: "€ / interval",
        height: 72,
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "net-cashflow",
            "Net operating cashflow",
            stepPoints(rows, (row) => row.netOperatingCashflow, stepMilliseconds),
            "dispatch-series-net-cashflow",
          ),
        ]),
      }),
      Object.freeze({
        key: "operating-cost",
        title: "Operating cost",
        unit: "€ / interval",
        height: 72,
        interpolation: "step",
        includeZero: true,
        series: Object.freeze([
          series(
            "operating-cost",
            "Operating cost",
            stepPoints(rows, (row) => row.operatingCost, stepMilliseconds),
            "dispatch-series-operating-cost",
          ),
        ]),
      }),
    ]),
  });
}

/**
 * @brief Computes a padded y-axis extent while preserving meaningful signs.
 * @param panel Panel descriptor with series and zero-inclusion policy.
 * @return Minimum and maximum y-axis values.
 */
export function extentForPanel(panel) {
  const values = panel.series.flatMap((item) => item.points.map((point) => point.value));
  let minimum = Math.min(...values);
  let maximum = Math.max(...values);

  // Ignore floating-point noise around zero without hiding real negative values.
  const magnitude = Math.max(Math.abs(minimum), Math.abs(maximum), 1);
  const zeroTolerance = magnitude * 1e-12;

  if (Math.abs(minimum) <= zeroTolerance) {
    minimum = 0;
  }
  if (Math.abs(maximum) <= zeroTolerance) {
    maximum = 0;
  }

  const dataAreNonnegative = minimum >= 0;
  const dataAreNonpositive = maximum <= 0;
  if (panel.includeZero) {
    minimum = Math.min(minimum, 0);
    maximum = Math.max(maximum, 0);
  }
  if (minimum === maximum) {
    const expansion = Math.max(Math.abs(minimum) * 0.1, 1);
    if (panel.includeZero && dataAreNonnegative) {
      minimum = 0;
      maximum += expansion;
    } else if (panel.includeZero && dataAreNonpositive) {
      minimum -= expansion;
      maximum = 0;
    } else {
      minimum -= expansion;
      maximum += expansion;
    }
  } else {
    const padding = (maximum - minimum) * 0.08;
    if (panel.includeZero && dataAreNonnegative) {
      minimum = 0;
      maximum += padding;
    } else if (panel.includeZero && dataAreNonpositive) {
      minimum -= padding;
      maximum = 0;
    } else {
      minimum -= padding;
      maximum += padding;
    }
  }
  return { minimum, maximum };
}

/**
 * @brief Creates a linear numeric mapping between domain and range intervals.
 * @param domainMinimum Lower input bound.
 * @param domainMaximum Upper input bound.
 * @param rangeMinimum Lower output bound.
 * @param rangeMaximum Upper output bound.
 * @return A function mapping domain values into the range.
 */
function linearScale(domainMinimum, domainMaximum, rangeMinimum, rangeMaximum) {
  const domainSpan = domainMaximum - domainMinimum;
  return (value) => rangeMinimum
    + ((value - domainMinimum) / domainSpan) * (rangeMaximum - rangeMinimum);
}

/**
 * @brief Clamps a requested chart time window to full bounds and minimum span.
 * @param startMilliseconds Requested start time.
 * @param endMilliseconds Requested end time.
 * @param fullStartMilliseconds Earliest available time.
 * @param fullEndMilliseconds Latest available time.
 * @param minimumSpanMilliseconds Smallest permitted visible duration.
 * @return A frozen valid start and end window.
 * @throws Error when bounds or spans are invalid.
 */
export function clampTimeDomain(
  startMilliseconds,
  endMilliseconds,
  fullStartMilliseconds,
  fullEndMilliseconds,
  minimumSpanMilliseconds,
) {
  requireCondition(
    [
      startMilliseconds,
      endMilliseconds,
      fullStartMilliseconds,
      fullEndMilliseconds,
      minimumSpanMilliseconds,
    ].every(Number.isFinite),
    "Chart time-domain values must be finite.",
  );
  requireCondition(fullEndMilliseconds > fullStartMilliseconds, "Chart bounds are invalid.");
  requireCondition(endMilliseconds > startMilliseconds, "Chart time domain is invalid.");
  requireCondition(minimumSpanMilliseconds > 0, "Chart minimum span must be positive.");

  const fullSpan = fullEndMilliseconds - fullStartMilliseconds;
  const minimumSpan = Math.min(minimumSpanMilliseconds, fullSpan);
  const span = Math.min(
    fullSpan,
    Math.max(minimumSpan, endMilliseconds - startMilliseconds),
  );
  let start = startMilliseconds;
  let end = start + span;
  if (start < fullStartMilliseconds) {
    start = fullStartMilliseconds;
    end = start + span;
  }
  if (end > fullEndMilliseconds) {
    end = fullEndMilliseconds;
    start = end - span;
  }
  return Object.freeze({ startMilliseconds: start, endMilliseconds: end });
}

/**
 * @brief Zooms a time window around an anchor while respecting chart bounds.
 * @param startMilliseconds Current visible start.
 * @param endMilliseconds Current visible end.
 * @param anchorMilliseconds Time that should remain under the pointer.
 * @param factor Scale factor below one to zoom in and above one to zoom out.
 * @param fullStartMilliseconds Earliest available time.
 * @param fullEndMilliseconds Latest available time.
 * @param minimumSpanMilliseconds Smallest permitted visible duration.
 * @return The clamped zoomed time window.
 */
export function zoomTimeDomain(
  startMilliseconds,
  endMilliseconds,
  anchorMilliseconds,
  factor,
  fullStartMilliseconds,
  fullEndMilliseconds,
  minimumSpanMilliseconds,
) {
  requireCondition(Number.isFinite(anchorMilliseconds), "Chart zoom anchor must be finite.");
  requireCondition(Number.isFinite(factor) && factor > 0, "Chart zoom factor must be positive.");
  const anchor = Math.min(
    endMilliseconds,
    Math.max(startMilliseconds, anchorMilliseconds),
  );
  const currentSpan = endMilliseconds - startMilliseconds;
  const effectiveFactor = Math.max(factor, minimumSpanMilliseconds / currentSpan);
  return clampTimeDomain(
    anchor - (anchor - startMilliseconds) * effectiveFactor,
    anchor + (endMilliseconds - anchor) * effectiveFactor,
    fullStartMilliseconds,
    fullEndMilliseconds,
    minimumSpanMilliseconds,
  );
}

/**
 * @brief Translates a time window while preserving its duration.
 * @param startMilliseconds Current visible start.
 * @param endMilliseconds Current visible end.
 * @param deltaMilliseconds Requested time translation.
 * @param fullStartMilliseconds Earliest available time.
 * @param fullEndMilliseconds Latest available time.
 * @return The clamped translated time window.
 */
export function panTimeDomain(
  startMilliseconds,
  endMilliseconds,
  deltaMilliseconds,
  fullStartMilliseconds,
  fullEndMilliseconds,
) {
  requireCondition(Number.isFinite(deltaMilliseconds), "Chart pan delta must be finite.");
  return clampTimeDomain(
    startMilliseconds + deltaMilliseconds,
    endMilliseconds + deltaMilliseconds,
    fullStartMilliseconds,
    fullEndMilliseconds,
    endMilliseconds - startMilliseconds,
  );
}

/**
 * @brief Builds an SVG path for line or step interpolation.
 * @param points Timestamped numeric points.
 * @param interpolation Either `line` or `step`.
 * @param xScale Timestamp-to-x mapping.
 * @param yScale Value-to-y mapping.
 * @return SVG path data.
 * @throws Error when the point sequence is empty.
 */
export function buildSeriesPath(points, interpolation, xScale, yScale) {
  requireCondition(points.length > 0, "A chart series must contain at least one point.");
  const first = points[0];
  const commands = [`M ${xScale(first.timestampMilliseconds)} ${yScale(first.value)}`];
  for (let index = 1; index < points.length; index += 1) {
    const point = points[index];
    if (interpolation === "step") {
      commands.push(`H ${xScale(point.timestampMilliseconds)}`);
      commands.push(`V ${yScale(point.value)}`);
    } else {
      commands.push(`L ${xScale(point.timestampMilliseconds)} ${yScale(point.value)}`);
    }
  }
  return commands.join(" ");
}

/**
 * @brief Creates an SVG element and assigns stringified attributes.
 * @param name SVG element name.
 * @param attributes Attribute map.
 * @return The configured SVG element.
 */
function svgElement(name, attributes = {}) {
  const element = document.createElementNS(SVG_NAMESPACE, name);
  for (const [key, value] of Object.entries(attributes)) {
    element.setAttribute(key, String(value));
  }
  return element;
}

/**
 * @brief Creates an SVG text element.
 * @param text Visible text.
 * @param attributes Attribute map.
 * @return The configured SVG text element.
 */
function svgText(text, attributes = {}) {
  const element = svgElement("text", attributes);
  element.textContent = text;
  return element;
}

/**
 * @brief Formats a y-axis value with magnitude-dependent precision.
 * @param value Axis value.
 * @return Localized tick text.
 */
function formatAxisNumber(value) {
  const absolute = Math.abs(value);
  const maximumFractionDigits = absolute >= 100 ? 0 : absolute >= 10 ? 1 : 2;
  return formatNumber(value, maximumFractionDigits);
}

/**
 * @brief Formats a tooltip value with bounded precision.
 * @param value Tooltip value.
 * @param maximumFractionDigits Maximum decimal places.
 * @return Localized numeric text.
 */
function formatTooltipNumber(value, maximumFractionDigits = 2) {
  return formatNumber(value, maximumFractionDigits);
}

/**
 * @brief Chooses an axis date format appropriate to the visible duration.
 * @param startMilliseconds Visible start time.
 * @param endMilliseconds Visible end time.
 * @return A Zurich-configured date-time formatter.
 */
function timeFormatter(startMilliseconds, endMilliseconds) {
  const durationDays = (endMilliseconds - startMilliseconds) / (24 * HOUR_MILLISECONDS);
  const options = durationDays > 370
    ? { timeZone: DISPATCH_CHART_TIME_ZONE, year: "numeric", month: "short" }
    : durationDays > 7
      ? { timeZone: DISPATCH_CHART_TIME_ZONE, month: "short", day: "2-digit" }
      : {
          timeZone: DISPATCH_CHART_TIME_ZONE,
          month: "short",
          day: "2-digit",
          hour: "2-digit",
          minute: "2-digit",
          hourCycle: "h23",
        };
  return new Intl.DateTimeFormat(undefined, options);
}

/**
 * @brief Creates the detailed Zurich timestamp formatter used by tooltips.
 * @return A date-time formatter.
 */
function tooltipTimeFormatter() {
  return new Intl.DateTimeFormat(undefined, {
    timeZone: DISPATCH_CHART_TIME_ZONE,
    dateStyle: "medium",
    timeStyle: "short",
  });
}

/**
 * @brief Appends one label/value row to the chart tooltip.
 * @param tooltip Tooltip container.
 * @param label Metric label.
 * @param value Formatted metric value.
 */
function appendTooltipLine(tooltip, label, value) {
  const line = document.createElement("div");
  const labelElement = document.createElement("span");
  labelElement.textContent = label;
  const valueElement = document.createElement("strong");
  valueElement.textContent = value;
  line.append(labelElement, valueElement);
  tooltip.append(line);
}

/**
 * @brief Selects an interval or the terminal state for a hover timestamp.
 * @param model Dispatch chart model.
 * @param timestampMilliseconds Hovered time.
 * @return An interval-row or terminal-state selection.
 * @throws Error when the timestamp is not finite.
 */
export function tooltipSelectionAtTimestamp(model, timestampMilliseconds) {
  requireCondition(Number.isFinite(timestampMilliseconds), "Tooltip timestamp must be finite.");
  if (timestampMilliseconds >= model.endMilliseconds) {
    return Object.freeze({
      kind: "terminal",
      timestampMilliseconds: model.endMilliseconds,
      storageContent: model.rows.at(-1).nextReservoirVolume,
    });
  }
  const rowIndex = Math.min(
    model.rows.length - 1,
    Math.max(0, Math.floor((timestampMilliseconds - model.startMilliseconds) / model.stepMilliseconds)),
  );
  return Object.freeze({ kind: "interval", row: model.rows[rowIndex] });
}

/**
 * @brief Renders synchronized SVG panels and interactive navigation controls.
 * @param container DOM container that receives the toolbar, stage, and tooltip.
 * @param model Chart model returned by `buildDispatchChartModel`.
 * @throws Error when the container is absent.
 */
export function renderDispatchCharts(container, model) {
  requireCondition(container, "A chart container is required.");
  container.replaceChildren();

  const viewWidth = 1200;
  const plotLeft = 200;
  const plotRight = 1170;
  const top = 18;
  const defaultPanelHeight = 104;
  const panelGap = 22;
  const bottomAxisHeight = 42;
  const plotWidth = plotRight - plotLeft;
  const fullSpan = model.endMilliseconds - model.startMilliseconds;
  const minimumSpan = Math.min(fullSpan, model.stepMilliseconds * 4);
  let currentDomain = Object.freeze({
    startMilliseconds: model.startMilliseconds,
    endMilliseconds: model.endMilliseconds,
  });
  let nextPanelTop = top;
  const panelLayouts = model.panels.map((panel) => {
    const panelHeight = panel.height ?? defaultPanelHeight;
    const panelTop = nextPanelTop;
    const panelBottom = panelTop + panelHeight;
    nextPanelTop = panelBottom + panelGap;
    return {
      panel,
      panelTop,
      panelBottom,
      panelHeight,
      extent: extentForPanel(panel),
    };
  });
  const viewHeight = panelLayouts.at(-1).panelBottom + bottomAxisHeight;
  const fullXScale = linearScale(
    model.startMilliseconds,
    model.endMilliseconds,
    plotLeft,
    plotRight,
  );

  const toolbar = document.createElement("div");
  toolbar.className = "dispatch-chart-toolbar";
  const guidance = document.createElement("span");
  guidance.className = "dispatch-chart-guidance";
  guidance.textContent = "Scroll to zoom · drag to pan · double-click to reset";
  const controls = document.createElement("div");
  controls.className = "dispatch-chart-controls";
  const rangeOutput = document.createElement("output");
  rangeOutput.className = "dispatch-chart-range";
  rangeOutput.setAttribute("aria-live", "polite");

/**
 * @brief Creates one chart navigation button.
 * @param label Visible button text.
 * @param accessibleLabel Accessible label and tooltip.
 * @return The configured button.
 */
  function chartButton(label, accessibleLabel) {
    const button = document.createElement("button");
    button.type = "button";
    button.className = "dispatch-chart-control-button";
    button.textContent = label;
    button.setAttribute("aria-label", accessibleLabel);
    button.title = accessibleLabel;
    return button;
  }

  const zoomInButton = chartButton("+", "Zoom in");
  const zoomOutButton = chartButton("−", "Zoom out");
  const resetButton = chartButton("Reset", "Reset chart zoom");
  controls.append(rangeOutput, zoomInButton, zoomOutButton, resetButton);
  toolbar.append(guidance, controls);

  const stage = document.createElement("div");
  stage.className = "dispatch-chart-stage";
  const tooltip = document.createElement("div");
  tooltip.className = "dispatch-chart-tooltip";
  tooltip.hidden = true;

  const renderId = ++chartRenderSequence;
  const svg = svgElement("svg", {
    class: "dispatch-chart-svg",
    viewBox: `0 0 ${viewWidth} ${viewHeight}`,
    role: "img",
    "aria-label": "Selected optimization dispatch charts",
  });
  const definitions = svgElement("defs");
  svg.append(definitions);

  for (const [layoutIndex, layout] of panelLayouts.entries()) {
    const { panel, panelTop, panelBottom, panelHeight, extent } = layout;
    const yScale = linearScale(extent.minimum, extent.maximum, panelBottom, panelTop);
    layout.yScale = yScale;

    svg.append(svgElement("rect", {
      class: "dispatch-chart-panel-background",
      x: plotLeft,
      y: panelTop,
      width: plotWidth,
      height: panelHeight,
      rx: 5,
    }));

    svg.append(svgText(panel.title, {
      class: "dispatch-chart-title",
      x: 8,
      y: panelTop + 17,
    }));
    svg.append(svgText(panel.unit, {
      class: "dispatch-chart-unit",
      x: 8,
      y: panelTop + 35,
    }));

    const tickValues = extent.minimum < 0 && extent.maximum > 0
      ? [extent.maximum, 0, extent.minimum]
      : [extent.maximum, (extent.maximum + extent.minimum) / 2, extent.minimum];
    for (const tickValue of tickValues) {
      const y = yScale(tickValue);
      svg.append(svgElement("line", {
        class: "dispatch-chart-grid-line",
        x1: plotLeft,
        x2: plotRight,
        y1: y,
        y2: y,
      }));
      svg.append(svgText(formatAxisNumber(tickValue), {
        class: "dispatch-chart-axis-label",
        x: plotLeft - 9,
        y: y + 4,
        "text-anchor": "end",
      }));
    }

    if (extent.minimum < 0 && extent.maximum > 0) {
      const zeroY = yScale(0);
      svg.append(svgElement("line", {
        class: "dispatch-chart-zero-line",
        x1: plotLeft,
        x2: plotRight,
        y1: zeroY,
        y2: zeroY,
      }));
    }

    // Clip each transformed series so synchronized pan and zoom cannot leave its panel.
    const clipId = `dispatch-chart-clip-${renderId}-${layoutIndex}`;
    const clipPath = svgElement("clipPath", { id: clipId });
    clipPath.append(svgElement("rect", {
      x: plotLeft,
      y: panelTop,
      width: plotWidth,
      height: panelHeight,
    }));
    definitions.append(clipPath);

    const clippedGroup = svgElement("g", { "clip-path": `url(#${clipId})` });
    const transformedGroup = svgElement("g");
    clippedGroup.append(transformedGroup);
    svg.append(clippedGroup);
    layout.transformedGroup = transformedGroup;

    panel.series.forEach((item, seriesIndex) => {
      transformedGroup.append(svgElement("path", {
        class: `dispatch-chart-series ${item.className}`,
        d: buildSeriesPath(item.points, panel.interpolation, fullXScale, yScale),
      }));
      if (panel.series.length > 1) {
        const legendX = plotLeft + 12 + seriesIndex * 132;
        const legendY = panelTop + 16;
        svg.append(svgElement("line", {
          class: `dispatch-chart-legend-line ${item.className}`,
          x1: legendX,
          x2: legendX + 18,
          y1: legendY,
          y2: legendY,
        }));
        svg.append(svgText(item.label, {
          class: "dispatch-chart-legend-label",
          x: legendX + 24,
          y: legendY + 4,
        }));
      }
    });
  }

  const axisY = panelLayouts.at(-1).panelBottom;
  const xAxisGroup = svgElement("g");
  svg.append(xAxisGroup);

  const hoverLine = svgElement("line", {
    class: "dispatch-chart-hover-line",
    x1: plotLeft,
    x2: plotLeft,
    y1: panelLayouts[0].panelTop,
    y2: panelLayouts.at(-1).panelBottom,
  });
  hoverLine.setAttribute("visibility", "hidden");
  svg.append(hoverLine);

  const terminalHoverExtension = Math.min(12, viewWidth - plotRight);
  const overlay = svgElement("rect", {
    class: "dispatch-chart-overlay",
    x: plotLeft,
    y: panelLayouts[0].panelTop,
    width: plotWidth + terminalHoverExtension,
    height: panelLayouts.at(-1).panelBottom - panelLayouts[0].panelTop,
    tabindex: 0,
    "aria-label": "Dispatch chart. Scroll to zoom, drag to pan, double-click to reset, and hover the right endpoint for the terminal state.",
  });
  svg.append(overlay);

  const timestampFormatter = tooltipTimeFormatter();
  const rangeFormatter = new Intl.DateTimeFormat(undefined, {
    timeZone: DISPATCH_CHART_TIME_ZONE,
    dateStyle: "medium",
    timeStyle: "short",
  });

/**
 * @brief Creates the x scale for the current synchronized time domain.
 * @return A timestamp-to-x mapping.
 */
  function visibleXScale() {
    return linearScale(
      currentDomain.startMilliseconds,
      currentDomain.endMilliseconds,
      plotLeft,
      plotRight,
    );
  }

/**
 * @brief Hides the hover line and tooltip.
 */
  function hideTooltip() {
    hoverLine.setAttribute("visibility", "hidden");
    tooltip.hidden = true;
  }

/**
 * @brief Checks whether two time windows have identical bounds.
 * @param first First domain.
 * @param second Second domain.
 * @return True when both bounds match.
 */
  function domainsEqual(first, second) {
    return first.startMilliseconds === second.startMilliseconds
      && first.endMilliseconds === second.endMilliseconds;
  }

/**
 * @brief Rebuilds time-axis ticks for the current domain.
 */
  function updateXAxis() {
    xAxisGroup.replaceChildren();
    const xFormatter = timeFormatter(
      currentDomain.startMilliseconds,
      currentDomain.endMilliseconds,
    );
    const xScale = visibleXScale();
    const xTickCount = 6;
    for (let index = 0; index < xTickCount; index += 1) {
      const fraction = index / (xTickCount - 1);
      const timestamp = currentDomain.startMilliseconds
        + fraction * (currentDomain.endMilliseconds - currentDomain.startMilliseconds);
      const x = xScale(timestamp);
      xAxisGroup.append(svgElement("line", {
        class: "dispatch-chart-axis-tick",
        x1: x,
        x2: x,
        y1: axisY,
        y2: axisY + 6,
      }));
      xAxisGroup.append(svgText(xFormatter.format(new Date(timestamp)), {
        class: "dispatch-chart-time-label",
        x,
        y: axisY + 23,
        "text-anchor": index === 0 ? "start" : index === xTickCount - 1 ? "end" : "middle",
      }));
    }
  }

/**
 * @brief Applies a synchronized domain to every panel and updates controls.
 * @param nextDomain Clamped visible time window.
 */
  function updateDomain(nextDomain) {
    currentDomain = nextDomain;
    const visibleSpan = currentDomain.endMilliseconds - currentDomain.startMilliseconds;
    const scale = fullSpan / visibleSpan;
    const offset = plotLeft - scale * fullXScale(currentDomain.startMilliseconds);
    const transform = `matrix(${scale} 0 0 1 ${offset} 0)`;
    for (const layout of panelLayouts) {
      layout.transformedGroup.setAttribute("transform", transform);
    }
    updateXAxis();
    rangeOutput.textContent = `${rangeFormatter.format(new Date(currentDomain.startMilliseconds))} – ${rangeFormatter.format(new Date(currentDomain.endMilliseconds))}`;
    const fullDomain = {
      startMilliseconds: model.startMilliseconds,
      endMilliseconds: model.endMilliseconds,
    };
    resetButton.disabled = domainsEqual(currentDomain, fullDomain);
    zoomOutButton.disabled = domainsEqual(currentDomain, fullDomain);
    zoomInButton.disabled = visibleSpan <= minimumSpan;
    hideTooltip();
  }

/**
 * @brief Applies an anchored zoom operation.
 * @param anchorMilliseconds Time to preserve under the pointer.
 * @param factor Zoom scale factor.
 */
  function zoomAround(anchorMilliseconds, factor) {
    updateDomain(zoomTimeDomain(
      currentDomain.startMilliseconds,
      currentDomain.endMilliseconds,
      anchorMilliseconds,
      factor,
      model.startMilliseconds,
      model.endMilliseconds,
      minimumSpan,
    ));
  }

/**
 * @brief Restores the full dispatch horizon.
 */
  function resetZoom() {
    updateDomain(Object.freeze({
      startMilliseconds: model.startMilliseconds,
      endMilliseconds: model.endMilliseconds,
    }));
  }

/**
 * @brief Maps a pointer position to the current visible timestamp.
 * @param event Pointer or wheel event.
 * @return Hovered epoch milliseconds.
 */
  function timestampAtPointer(event) {
    const bounds = svg.getBoundingClientRect();
    const viewX = ((event.clientX - bounds.left) / bounds.width) * viewWidth;
    const clampedX = Math.min(plotRight, Math.max(plotLeft, viewX));
    const fraction = (clampedX - plotLeft) / plotWidth;
    return currentDomain.startMilliseconds
      + fraction * (currentDomain.endMilliseconds - currentDomain.startMilliseconds);
  }

/**
 * @brief Updates the synchronized hover line and tooltip content.
 * @param event Pointer event over the chart overlay.
 */
  function showTooltip(event) {
    const selection = tooltipSelectionAtTimestamp(model, timestampAtPointer(event));
    const selectionTimestamp = selection.kind === "terminal"
      ? selection.timestampMilliseconds
      : selection.row.timestampMilliseconds;
    const selectionX = selection.kind === "terminal"
      ? plotRight
      : Math.min(
        plotRight,
        Math.max(plotLeft, visibleXScale()(selectionTimestamp)),
      );
    hoverLine.setAttribute("x1", String(selectionX));
    hoverLine.setAttribute("x2", String(selectionX));
    hoverLine.setAttribute("visibility", "visible");

    tooltip.replaceChildren();
    const heading = document.createElement("strong");
    heading.className = "dispatch-chart-tooltip-heading";
    heading.textContent = timestampFormatter.format(new Date(selectionTimestamp));
    tooltip.append(heading);
    if (selection.kind === "terminal") {
      appendTooltipLine(tooltip, "State", "Terminal boundary");
      appendTooltipLine(
        tooltip,
        "Storage content [MWh hydraulic]",
        formatTooltipNumber(selection.storageContent),
      );
    } else {
      const row = selection.row;
      appendTooltipLine(tooltip, "Mode", operatingMode(row));
      appendTooltipLine(tooltip, "Price [€/MWh]", formatTooltipNumber(row.price));
      appendTooltipLine(tooltip, "Inflow [MW hydraulic]", formatTooltipNumber(row.naturalInflow));
      appendTooltipLine(tooltip, "Turbine withdrawal [MW hydraulic]", formatTooltipNumber(row.turbineFlow));
      appendTooltipLine(tooltip, "Pump addition [MW hydraulic]", formatTooltipNumber(row.pumpFlow));
      appendTooltipLine(tooltip, "Spill [MW hydraulic]", formatTooltipNumber(row.spillFlow));
      appendTooltipLine(tooltip, "Storage content [MWh hydraulic]", formatTooltipNumber(row.reservoirVolume));
      appendTooltipLine(tooltip, "Next storage content [MWh hydraulic]", formatTooltipNumber(row.nextReservoirVolume));
      appendTooltipLine(tooltip, "Net power [MW]", formatTooltipNumber(row.netPower));
      appendTooltipLine(tooltip, "Market settlement [€]", formatTooltipNumber(row.marketCashflow));
      appendTooltipLine(tooltip, "Operating cost [€]", formatTooltipNumber(row.operatingCost));
      appendTooltipLine(tooltip, "Net operating cashflow [€]", formatTooltipNumber(row.netOperatingCashflow));
    }
    tooltip.hidden = false;
    const renderedWidth = svg.getBoundingClientRect().width;
    const tooltipX = (selectionX / viewWidth) * renderedWidth;
    tooltip.style.left = `${Math.min(renderedWidth - 110, Math.max(110, tooltipX))}px`;
  }

  // Pointer movement is translated into a time-domain delta while dragging.
  let panState = null;
  overlay.addEventListener("pointerdown", (event) => {
    if (event.button !== 0) {
      return;
    }
    panState = {
      pointerId: event.pointerId,
      clientX: event.clientX,
      startMilliseconds: currentDomain.startMilliseconds,
      endMilliseconds: currentDomain.endMilliseconds,
    };
    overlay.setPointerCapture(event.pointerId);
    overlay.classList.add("dispatch-chart-panning");
    hideTooltip();
  });
  overlay.addEventListener("pointermove", (event) => {
    if (panState && panState.pointerId === event.pointerId) {
      const bounds = svg.getBoundingClientRect();
      const renderedPlotWidth = (plotWidth / viewWidth) * bounds.width;
      const span = panState.endMilliseconds - panState.startMilliseconds;
      const delta = -((event.clientX - panState.clientX) / renderedPlotWidth) * span;
      updateDomain(panTimeDomain(
        panState.startMilliseconds,
        panState.endMilliseconds,
        delta,
        model.startMilliseconds,
        model.endMilliseconds,
      ));
      event.preventDefault();
      return;
    }
    showTooltip(event);
  });

/**
 * @brief Ends pointer capture and clears panning state.
 * @param event Pointer event ending the drag.
 */
  function finishPan(event) {
    if (!panState || panState.pointerId !== event.pointerId) {
      return;
    }
    if (overlay.hasPointerCapture(event.pointerId)) {
      overlay.releasePointerCapture(event.pointerId);
    }
    panState = null;
    overlay.classList.remove("dispatch-chart-panning");
  }

  overlay.addEventListener("pointerup", finishPan);
  overlay.addEventListener("pointercancel", finishPan);
  overlay.addEventListener("pointerleave", () => {
    if (!panState) {
      hideTooltip();
    }
  });
  overlay.addEventListener("wheel", (event) => {
    event.preventDefault();
    const boundedDelta = Math.max(-300, Math.min(300, event.deltaY));
    zoomAround(timestampAtPointer(event), Math.exp(boundedDelta * 0.002));
  }, { passive: false });
  overlay.addEventListener("dblclick", resetZoom);
  overlay.addEventListener("keydown", (event) => {
    const midpoint = (currentDomain.startMilliseconds + currentDomain.endMilliseconds) / 2;
    const span = currentDomain.endMilliseconds - currentDomain.startMilliseconds;
    if (event.key === "+" || event.key === "=") {
      zoomAround(midpoint, 0.5);
    } else if (event.key === "-") {
      zoomAround(midpoint, 2);
    } else if (event.key === "0" || event.key === "Home") {
      resetZoom();
    } else if (event.key === "ArrowLeft") {
      updateDomain(panTimeDomain(
        currentDomain.startMilliseconds,
        currentDomain.endMilliseconds,
        -span * 0.1,
        model.startMilliseconds,
        model.endMilliseconds,
      ));
    } else if (event.key === "ArrowRight") {
      updateDomain(panTimeDomain(
        currentDomain.startMilliseconds,
        currentDomain.endMilliseconds,
        span * 0.1,
        model.startMilliseconds,
        model.endMilliseconds,
      ));
    } else {
      return;
    }
    event.preventDefault();
  });

  zoomInButton.addEventListener("click", () => {
    zoomAround(
      (currentDomain.startMilliseconds + currentDomain.endMilliseconds) / 2,
      0.5,
    );
  });
  zoomOutButton.addEventListener("click", () => {
    zoomAround(
      (currentDomain.startMilliseconds + currentDomain.endMilliseconds) / 2,
      2,
    );
  });
  resetButton.addEventListener("click", resetZoom);

  stage.append(svg, tooltip);
  container.append(toolbar, stage);
  updateDomain(currentDomain);
}
