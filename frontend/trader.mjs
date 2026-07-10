const PRODUCT_KEYS = Object.freeze(["baseload", "peak", "offPeak"]);
export const TRADER_TIME_ZONE = "Europe/Zurich";
export const PEAK_START_HOUR = 9;
export const PEAK_END_HOUR = 20;
const WEEKDAYS = new Set(["Mon", "Tue", "Wed", "Thu", "Fri"]);
const HOUR_MILLISECONDS = 3_600_000;

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

export function parseDispatchCsv(text) {
  const lines = String(text).split(/\r?\n/);
  const header = (lines.shift() ?? "").split(",").map((item) => item.trim());
  const positions = Object.fromEntries(header.map((name, index) => [name, index]));
  for (const required of ["time_index", "net_power", "reward"]) {
    requireCondition(required in positions, `Dispatch CSV is missing ${required}.`);
  }

  const rows = [];
  let expectedIndex = 0;
  for (let lineIndex = 0; lineIndex < lines.length; lineIndex += 1) {
    const line = lines[lineIndex].trim();
    if (!line) {
      continue;
    }
    const columns = line.split(",").map((item) => item.trim());
    const rowNumber = lineIndex + 2;
    requireCondition(columns.length === header.length, `Dispatch row ${rowNumber} has the wrong column count.`);
    const timeIndex = Number(columns[positions.time_index]);
    requireCondition(
      Number.isSafeInteger(timeIndex) && timeIndex === expectedIndex,
      `Dispatch row ${rowNumber} must use time_index ${expectedIndex}.`,
    );
    rows.push(Object.freeze({
      timeIndex,
      netPowerMw: parseFinite(columns[positions.net_power], "net_power", rowNumber),
      reward: parseFinite(columns[positions.reward], "reward", rowNumber),
    }));
    expectedIndex += 1;
  }
  requireCondition(rows.length > 0, "Dispatch CSV contains no data rows.");
  return Object.freeze(rows);
}

function formatterFor(timeZone) {
  try {
    return new Intl.DateTimeFormat("en-CA", {
      timeZone,
      year: "numeric",
      month: "2-digit",
      day: "2-digit",
      weekday: "short",
      hour: "2-digit",
      hourCycle: "h23",
    });
  } catch {
    throw new Error(`Unsupported market timezone: ${timeZone}.`);
  }
}

function localParts(formatter, timestamp) {
  const values = Object.fromEntries(
    formatter.formatToParts(timestamp)
      .filter((part) => part.type !== "literal")
      .map((part) => [part.type, part.value]),
  );
  return {
    year: Number(values.year),
    month: Number(values.month),
    day: Number(values.day),
    hour: Number(values.hour),
    weekday: values.weekday,
  };
}

function monthName(year, month) {
  return new Intl.DateTimeFormat("en", { month: "long", timeZone: "UTC" })
    .format(new Date(Date.UTC(year, month - 1, 1)));
}

function periodFor(parts, startParts) {
  const monthIndex = parts.year * 12 + (parts.month - 1);
  const startMonthIndex = startParts.year * 12 + (startParts.month - 1);
  if (monthIndex - startMonthIndex < 12) {
    return {
      key: `M-${parts.year}-${String(parts.month).padStart(2, "0")}`,
      label: `${monthName(parts.year, parts.month)} ${parts.year}`,
      order: monthIndex,
    };
  }
  const quarter = Math.floor((parts.month - 1) / 3) + 1;
  return {
    key: `Q-${parts.year}-${quarter}`,
    label: `Q${quarter} ${parts.year}`,
    order: parts.year * 12 + (quarter - 1) * 3,
  };
}

function validateTimeline(timeline) {
  requireCondition(timeline, "Scenario has no series timeline.");
  const startMilliseconds = Date.parse(timeline.series_start_utc);
  requireCondition(Number.isFinite(startMilliseconds), "Scenario series start is invalid.");
  const timeStepHours = Number(timeline.time_step_hours);
  requireCondition(Number.isFinite(timeStepHours) && timeStepHours > 0, "Scenario time step is invalid.");
  return { startMilliseconds, timeStepHours };
}

function emptyProductAggregate() {
  return { hours: 0, energyMwh: 0, pnl: 0 };
}

function finalizedProduct(aggregate) {
  return Object.freeze({
    averageMw: aggregate.hours > 0 ? aggregate.energyMwh / aggregate.hours : null,
    energyMwh: aggregate.energyMwh,
    pnl: aggregate.pnl,
  });
}

export function buildTraderRows(dispatchText, timeline) {
  const dispatch = parseDispatchCsv(dispatchText);
  const { startMilliseconds, timeStepHours } = validateTimeline(timeline);
  const formatter = formatterFor(TRADER_TIME_ZONE);
  const startParts = localParts(formatter, new Date(startMilliseconds));
  const periods = new Map();

  function ensurePeriod(period) {
    if (!periods.has(period.key)) {
      periods.set(period.key, {
        period: period.label,
        periodOrder: period.order,
        products: Object.fromEntries(
          PRODUCT_KEYS.map((productKey) => [productKey, emptyProductAggregate()]),
        ),
      });
    }
    return periods.get(period.key);
  }

  function add(periodAggregate, productKey, row, durationHours, pnl) {
    const product = periodAggregate.products[productKey];
    product.hours += durationHours;
    product.energyMwh += row.netPowerMw * durationHours;
    product.pnl += pnl;
  }

  for (const row of dispatch) {
    const intervalStart = startMilliseconds + row.timeIndex * timeStepHours * HOUR_MILLISECONDS;
    const intervalEnd = intervalStart + timeStepHours * HOUR_MILLISECONDS;
    let segmentStart = intervalStart;

    while (segmentStart < intervalEnd) {
      const nextHourBoundary = (Math.floor(segmentStart / HOUR_MILLISECONDS) + 1)
        * HOUR_MILLISECONDS;
      const segmentEnd = Math.min(intervalEnd, nextHourBoundary);
      const durationHours = (segmentEnd - segmentStart) / HOUR_MILLISECONDS;
      const pnl = row.reward * (durationHours / timeStepHours);
      const parts = localParts(formatter, new Date(segmentStart));
      const periodAggregate = ensurePeriod(periodFor(parts, startParts));
      const isPeak = WEEKDAYS.has(parts.weekday)
        && parts.hour >= PEAK_START_HOUR
        && parts.hour < PEAK_END_HOUR;
      add(periodAggregate, "baseload", row, durationHours, pnl);
      add(periodAggregate, isPeak ? "peak" : "offPeak", row, durationHours, pnl);
      segmentStart = segmentEnd;
    }
  }

  return Object.freeze(
    [...periods.values()]
      .sort((left, right) => left.periodOrder - right.periodOrder)
      .map((aggregate) => Object.freeze({
        period: aggregate.period,
        baseload: finalizedProduct(aggregate.products.baseload),
        peak: finalizedProduct(aggregate.products.peak),
        offPeak: finalizedProduct(aggregate.products.offPeak),
      })),
  );
}
