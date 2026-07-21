/**
 * @file
 * @brief Dispatch aggregation into Baseload, Peak, and Off-peak reporting products.
 *
 * Product classification uses Europe/Zurich civil time. Optimizer intervals are
 * split at hourly boundaries so Peak and Off-peak allocations remain correct for
 * multi-hour scenarios and daylight-saving transitions.
 */

/**
 * @brief Internal product keys initialized for each reporting period.
 */
const PRODUCT_KEYS = Object.freeze(["baseload", "peak", "offPeak"]);
/**
 * @brief Civil time zone used for product and reporting-period classification.
 */
export const TRADER_TIME_ZONE = "Europe/Zurich";
/**
 * @brief Inclusive weekday Peak start hour in local civil time.
 */
export const PEAK_START_HOUR = 9;
/**
 * @brief Exclusive weekday Peak end hour in local civil time.
 */
export const PEAK_END_HOUR = 20;
/** Weekday abbreviations emitted by the fixed English formatter. */
const WEEKDAYS = new Set(["Mon", "Tue", "Wed", "Thu", "Fri"]);
/** Milliseconds per hour used to split and prorate intervals. */
const HOUR_MILLISECONDS = 3_600_000;
/** Canonical whole-second UTC timestamp accepted from dispatch artifacts. */
const UTC_TIMESTAMP_PATTERN = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$/;

/**
 * @brief Throws a dispatch-validation error when a condition is false.
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
 * @brief Parses a canonical UTC dispatch timestamp.
 * @param value Timestamp text.
 * @param rowNumber One-based CSV row number.
 * @return Timestamp text and epoch milliseconds.
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
  return { timestampUtc: value, timestampMilliseconds: milliseconds };
}

/**
 * @brief Parses the dispatch columns required by trader aggregation.
 * @param text Dispatch CSV text.
 * @return An immutable sequence of timestamped net-power and reward rows.
 * @throws Error when the artifact is malformed.
 */
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

/**
 * @brief Creates the reusable formatter used to extract Zurich calendar parts.
 * @return An `Intl.DateTimeFormat` configured for product classification.
 */
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

/**
 * @brief Extracts numeric Zurich calendar fields from a timestamp.
 * @param formatter Zurich date-time formatter.
 * @param timestamp JavaScript Date instance.
 * @return Year, month, day, hour, and abbreviated weekday.
 */
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

/**
 * @brief Formats an English calendar month name.
 * @param year Calendar year.
 * @param month One-based calendar month.
 * @return The month name.
 */
function monthName(year, month) {
  return new Intl.DateTimeFormat("en", { month: "long", timeZone: "UTC" })
    .format(new Date(Date.UTC(year, month - 1, 1)));
}

/**
 * @brief Selects a monthly or quarterly reporting bucket.
 * @param parts Local calendar parts for the interval segment.
 * @param startParts Local calendar parts for the dispatch start.
 * @return Stable period key, display label, and sort order.
 */
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
 * @brief Creates a mutable accumulator for one product.
 * @return Zeroed hours, energy, and cashflow totals.
 */
function emptyProductAggregate() {
  return { hours: 0, energyMwh: 0, cashflow: 0 };
}

/**
 * @brief Converts an internal accumulator into an immutable reported product.
 * @param aggregate Mutable product totals.
 * @return Average power, signed energy, and cashflow.
 */
function finalizedProduct(aggregate) {
  return Object.freeze({
    averageMw: aggregate.hours > 0 ? aggregate.energyMwh / aggregate.hours : null,
    energyMwh: aggregate.energyMwh,
    cashflow: aggregate.cashflow,
  });
}

/**
 * @brief Aggregates a dispatch artifact into ordered reporting-period rows.
 * @param dispatchText Dispatch CSV text.
 * @param timeStepHours Scenario interval duration in hours.
 * @return Immutable monthly and quarterly product rows.
 * @throws Error when dispatch spacing is inconsistent.
 */
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

/**
 * @brief Returns the accumulator for a period, creating it on first use.
 * @param period Period descriptor returned by `periodFor`.
 * @return The mutable period accumulator.
 */
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

/**
 * @brief Adds one interval segment to a product accumulator.
 * @param periodAggregate Containing period accumulator.
 * @param productKey Baseload, Peak, or Off-peak key.
 * @param row Parsed dispatch row.
 * @param durationHours Segment duration in hours.
 * @param cashflow Reward allocated to the segment.
 */
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

    // Split intervals at civil-hour boundaries before Peak classification.
    while (segmentStart < intervalEnd) {
      const nextHourBoundary = (Math.floor(segmentStart / HOUR_MILLISECONDS) + 1)
        * HOUR_MILLISECONDS;
      const segmentEnd = Math.min(intervalEnd, nextHourBoundary);
      const durationHours = (segmentEnd - segmentStart) / HOUR_MILLISECONDS;
      // Reward is an interval total, so allocate it in proportion to segment duration.
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
