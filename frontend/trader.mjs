const PRODUCT_KEYS = Object.freeze(["baseload", "peak", "offPeak"]);
export const TRADER_TIME_ZONE = "Europe/Zurich";
export const PEAK_START_HOUR = 9;
export const PEAK_END_HOUR = 20;
const WEEKDAYS = new Set(["Mon", "Tue", "Wed", "Thu", "Fri"]);
const HOUR_MILLISECONDS = 3_600_000;
const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;

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
  return { timestampUtc: value, timestampMilliseconds: milliseconds };
}

export function parseDispatchCsv(text) {
  const lines = String(text).split(/\r?\n/);
  const header = (lines.shift() ?? "").split(",").map((item) => item.trim());
  const positions = Object.fromEntries(header.map((name, index) => [name, index]));
  for (const required of ["time_index", "timestamp_utc", "net_power", "reward"]) {
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
    const timestamp = parseTimestampUtc(columns[positions.timestamp_utc], rowNumber);
    if (rows.length > 0) {
      requireCondition(
        timestamp.timestampMilliseconds > rows.at(-1).timestampMilliseconds,
        `Dispatch row ${rowNumber} timestamp_utc must be strictly increasing.`,
      );
    }
    rows.push(Object.freeze({
      timeIndex,
      ...timestamp,
      netPowerMw: parseFinite(columns[positions.net_power], "net_power", rowNumber),
      reward: parseFinite(columns[positions.reward], "reward", rowNumber),
    }));
    expectedIndex += 1;
  }
  requireCondition(rows.length > 0, "Dispatch CSV contains no data rows.");
  return Object.freeze(rows);
}

function formatterForZurich() {
  return new Intl.DateTimeFormat("en-CA", {
    timeZone: TRADER_TIME_ZONE,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    weekday: "short",
    hour: "2-digit",
    hourCycle: "h23",
  });
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

function emptyProductAggregate() {
  return { hours: 0, energyMwh: 0, cashflow: 0 };
}

function finalizedProduct(aggregate) {
  return Object.freeze({
    averageMw: aggregate.hours > 0 ? aggregate.energyMwh / aggregate.hours : null,
    energyMwh: aggregate.energyMwh,
    cashflow: aggregate.cashflow,
  });
}

export function buildTraderRows(dispatchText, timeStepHours) {
  const dispatch = parseDispatchCsv(dispatchText);
  const stepMilliseconds = intervalMilliseconds(timeStepHours);
  const timeStep = stepMilliseconds / HOUR_MILLISECONDS;
  for (let index = 1; index < dispatch.length; index += 1) {
    requireCondition(
      dispatch[index].timestampMilliseconds - dispatch[index - 1].timestampMilliseconds
        === stepMilliseconds,
      `Dispatch timestamp spacing at row ${index + 2} does not match time_step_hours.`,
    );
  }

  const formatter = formatterForZurich();
  const startParts = localParts(formatter, new Date(dispatch[0].timestampMilliseconds));
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

  function add(periodAggregate, productKey, row, durationHours, cashflow) {
    const product = periodAggregate.products[productKey];
    product.hours += durationHours;
    product.energyMwh += row.netPowerMw * durationHours;
    product.cashflow += cashflow;
  }

  for (const row of dispatch) {
    const intervalStart = row.timestampMilliseconds;
    const intervalEnd = intervalStart + stepMilliseconds;
    let segmentStart = intervalStart;

    while (segmentStart < intervalEnd) {
      const nextHourBoundary = (Math.floor(segmentStart / HOUR_MILLISECONDS) + 1)
        * HOUR_MILLISECONDS;
      const segmentEnd = Math.min(intervalEnd, nextHourBoundary);
      const durationHours = (segmentEnd - segmentStart) / HOUR_MILLISECONDS;
      const cashflow = row.reward * (durationHours / timeStep);
      const parts = localParts(formatter, new Date(segmentStart));
      const periodAggregate = ensurePeriod(periodFor(parts, startParts));
      const isPeak = WEEKDAYS.has(parts.weekday)
        && parts.hour >= PEAK_START_HOUR
        && parts.hour < PEAK_END_HOUR;
      add(periodAggregate, "baseload", row, durationHours, cashflow);
      add(periodAggregate, isPeak ? "peak" : "offPeak", row, durationHours, cashflow);
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
