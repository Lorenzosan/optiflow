import { formatNumber } from "./number_format.mjs";

const HOUR_MILLISECONDS = 3_600_000;
const SVG_NAMESPACE = "http://www.w3.org/2000/svg";
const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;
let chartRenderSequence = 0;

export const DISPATCH_CHART_TIME_ZONE = "Europe/Zurich";

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
  "cumulative_profit",
]);

function requireCondition(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function parseFinite(value, label, rowNumber) {
  const parsed = Number(value);
  requireCondition(Number.isFinite(parsed), `Dispatch row ${rowNumber} has an invalid ${label}.`);
  return parsed;
}

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

function linePoints(rows, valueForRow) {
  return Object.freeze(rows.map((row) => Object.freeze({
    timestampMilliseconds: row.timestampMilliseconds,
    value: valueForRow(row),
  })));
}

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

function series(key, label, points, className) {
  return Object.freeze({ key, label, points, className });
}

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
        includeZero: false,
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
        includeZero: false,
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

export function extentForPanel(panel) {
  const values = panel.series.flatMap((item) => item.points.map((point) => point.value));
  let minimum = Math.min(...values);
  let maximum = Math.max(...values);
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

function linearScale(domainMinimum, domainMaximum, rangeMinimum, rangeMaximum) {
  const domainSpan = domainMaximum - domainMinimum;
  return (value) => rangeMinimum
    + ((value - domainMinimum) / domainSpan) * (rangeMaximum - rangeMinimum);
}

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

function svgElement(name, attributes = {}) {
  const element = document.createElementNS(SVG_NAMESPACE, name);
  for (const [key, value] of Object.entries(attributes)) {
    element.setAttribute(key, String(value));
  }
  return element;
}

function svgText(text, attributes = {}) {
  const element = svgElement("text", attributes);
  element.textContent = text;
  return element;
}

function formatAxisNumber(value) {
  const absolute = Math.abs(value);
  const maximumFractionDigits = absolute >= 100 ? 0 : absolute >= 10 ? 1 : 2;
  return formatNumber(value, maximumFractionDigits);
}

function formatTooltipNumber(value, maximumFractionDigits = 2) {
  return formatNumber(value, maximumFractionDigits);
}

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

function tooltipTimeFormatter() {
  return new Intl.DateTimeFormat(undefined, {
    timeZone: DISPATCH_CHART_TIME_ZONE,
    dateStyle: "medium",
    timeStyle: "short",
  });
}

function appendTooltipLine(tooltip, label, value) {
  const line = document.createElement("div");
  const labelElement = document.createElement("span");
  labelElement.textContent = label;
  const valueElement = document.createElement("strong");
  valueElement.textContent = value;
  line.append(labelElement, valueElement);
  tooltip.append(line);
}

export function renderDispatchCharts(container, model) {
  requireCondition(container, "A chart container is required.");
  container.replaceChildren();

  const viewWidth = 1120;
  const plotLeft = 132;
  const plotRight = 1090;
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

  const overlay = svgElement("rect", {
    class: "dispatch-chart-overlay",
    x: plotLeft,
    y: panelLayouts[0].panelTop,
    width: plotWidth,
    height: panelLayouts.at(-1).panelBottom - panelLayouts[0].panelTop,
    tabindex: 0,
    "aria-label": "Dispatch chart. Scroll to zoom, drag to pan, and double-click to reset.",
  });
  svg.append(overlay);

  const timestampFormatter = tooltipTimeFormatter();
  const rangeFormatter = new Intl.DateTimeFormat(undefined, {
    timeZone: DISPATCH_CHART_TIME_ZONE,
    dateStyle: "medium",
    timeStyle: "short",
  });

  function visibleXScale() {
    return linearScale(
      currentDomain.startMilliseconds,
      currentDomain.endMilliseconds,
      plotLeft,
      plotRight,
    );
  }

  function hideTooltip() {
    hoverLine.setAttribute("visibility", "hidden");
    tooltip.hidden = true;
  }

  function domainsEqual(first, second) {
    return first.startMilliseconds === second.startMilliseconds
      && first.endMilliseconds === second.endMilliseconds;
  }

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

  function resetZoom() {
    updateDomain(Object.freeze({
      startMilliseconds: model.startMilliseconds,
      endMilliseconds: model.endMilliseconds,
    }));
  }

  function timestampAtPointer(event) {
    const bounds = svg.getBoundingClientRect();
    const viewX = ((event.clientX - bounds.left) / bounds.width) * viewWidth;
    const clampedX = Math.min(plotRight, Math.max(plotLeft, viewX));
    const fraction = (clampedX - plotLeft) / plotWidth;
    return currentDomain.startMilliseconds
      + fraction * (currentDomain.endMilliseconds - currentDomain.startMilliseconds);
  }

  function showTooltip(event) {
    const timestamp = timestampAtPointer(event);
    const rowIndex = Math.min(
      model.rows.length - 1,
      Math.max(0, Math.floor((timestamp - model.startMilliseconds) / model.stepMilliseconds)),
    );
    const row = model.rows[rowIndex];
    const rowX = Math.min(
      plotRight,
      Math.max(plotLeft, visibleXScale()(row.timestampMilliseconds)),
    );
    hoverLine.setAttribute("x1", String(rowX));
    hoverLine.setAttribute("x2", String(rowX));
    hoverLine.setAttribute("visibility", "visible");

    tooltip.replaceChildren();
    const heading = document.createElement("strong");
    heading.className = "dispatch-chart-tooltip-heading";
    heading.textContent = timestampFormatter.format(new Date(row.timestampMilliseconds));
    tooltip.append(heading);
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
    tooltip.hidden = false;
    const renderedWidth = svg.getBoundingClientRect().width;
    const tooltipX = (rowX / viewWidth) * renderedWidth;
    tooltip.style.left = `${Math.min(renderedWidth - 110, Math.max(110, tooltipX))}px`;
  }

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
