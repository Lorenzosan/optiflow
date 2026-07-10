from __future__ import annotations

import csv
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import math
from pathlib import Path
import re


REPORTING_KEYS = frozenset(
    {
        "market_start_utc",
        "market_timezone",
        "peak_start_hour",
        "peak_end_hour",
    }
)
_TIMEZONE_PATTERN = re.compile(r"^(?:UTC|[A-Za-z_]+(?:/[A-Za-z0-9_.+-]+)+)$")


class ScenarioReportingError(ValueError):
    """Scenario reporting metadata is incomplete or malformed."""


@dataclass(frozen=True)
class ScenarioReportingMetadata:
    market_start_utc: datetime
    market_timezone: str
    peak_start_hour: int
    peak_end_hour: int
    time_step_hours: float


def _read_key_values(path: Path) -> dict[str, str]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != ["key", "value"]:
                raise ScenarioReportingError("scenario file must use the header key,value")

            values: dict[str, str] = {}
            for row_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    raise ScenarioReportingError(
                        f"scenario row {row_number} must contain exactly two columns"
                    )
                key = row[0].strip()
                value = row[1].strip()
                if key in values:
                    raise ScenarioReportingError(f"scenario key {key} is duplicated")
                values[key] = value
            return values
    except ScenarioReportingError:
        raise
    except (OSError, UnicodeError) as error:
        raise ScenarioReportingError("scenario file must be readable UTF-8 CSV") from error


def _parse_utc_datetime(value: str) -> datetime:
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise ScenarioReportingError("market_start_utc must be an ISO-8601 datetime") from error
    if parsed.tzinfo is None or parsed.utcoffset() != timedelta(0):
        raise ScenarioReportingError("market_start_utc must use UTC (Z or +00:00)")
    return parsed.astimezone(timezone.utc)


def _parse_hour(values: dict[str, str], key: str) -> int:
    text = values[key]
    if not text.isdigit():
        raise ScenarioReportingError(f"{key} must be an integer hour")
    return int(text)


def load_scenario_reporting(path: Path) -> ScenarioReportingMetadata | None:
    values = _read_key_values(path)
    present = REPORTING_KEYS.intersection(values)
    if not present:
        return None

    missing = REPORTING_KEYS.difference(values)
    if missing:
        raise ScenarioReportingError(
            "scenario reporting metadata is incomplete; missing " + ", ".join(sorted(missing))
        )

    timezone_name = values["market_timezone"]
    if not _TIMEZONE_PATTERN.fullmatch(timezone_name):
        raise ScenarioReportingError("market_timezone must be UTC or an IANA-style timezone")

    peak_start = _parse_hour(values, "peak_start_hour")
    peak_end = _parse_hour(values, "peak_end_hour")
    if not 0 <= peak_start <= 23:
        raise ScenarioReportingError("peak_start_hour must be between 0 and 23")
    if not 1 <= peak_end <= 24:
        raise ScenarioReportingError("peak_end_hour must be between 1 and 24")
    if peak_start >= peak_end:
        raise ScenarioReportingError("peak_start_hour must be earlier than peak_end_hour")

    try:
        time_step_hours = float(values["time_step_hours"])
    except (KeyError, ValueError) as error:
        raise ScenarioReportingError(
            "time_step_hours is required for scenario reporting metadata"
        ) from error
    if not math.isfinite(time_step_hours) or time_step_hours <= 0:
        raise ScenarioReportingError("time_step_hours must be finite and positive")

    return ScenarioReportingMetadata(
        market_start_utc=_parse_utc_datetime(values["market_start_utc"]),
        market_timezone=timezone_name,
        peak_start_hour=peak_start,
        peak_end_hour=peak_end,
        time_step_hours=time_step_hours,
    )
