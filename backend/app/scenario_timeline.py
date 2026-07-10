from __future__ import annotations

import csv
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import math
from pathlib import Path


SERIES_START_KEY = "series_start_utc"


class ScenarioTimelineError(ValueError):
    """Scenario timeline metadata is malformed."""


@dataclass(frozen=True)
class ScenarioTimelineMetadata:
    series_start_utc: datetime
    time_step_hours: float


def _read_key_values(path: Path) -> dict[str, str]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != ["key", "value"]:
                raise ScenarioTimelineError("scenario file must use the header key,value")

            values: dict[str, str] = {}
            for row_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    raise ScenarioTimelineError(
                        f"scenario row {row_number} must contain exactly two columns"
                    )
                key = row[0].strip()
                value = row[1].strip()
                if key in values:
                    raise ScenarioTimelineError(f"scenario key {key} is duplicated")
                values[key] = value
            return values
    except ScenarioTimelineError:
        raise
    except (OSError, UnicodeError) as error:
        raise ScenarioTimelineError("scenario file must be readable UTF-8 CSV") from error


def _parse_utc_datetime(value: str) -> datetime:
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as error:
        raise ScenarioTimelineError(
            "series_start_utc must be an ISO-8601 datetime"
        ) from error
    if parsed.tzinfo is None or parsed.utcoffset() != timedelta(0):
        raise ScenarioTimelineError("series_start_utc must use UTC (Z or +00:00)")
    return parsed.astimezone(timezone.utc)


def load_scenario_timeline(path: Path) -> ScenarioTimelineMetadata:
    values = _read_key_values(path)
    if SERIES_START_KEY not in values:
        raise ScenarioTimelineError("scenario file is missing series_start_utc")

    try:
        time_step_hours = float(values["time_step_hours"])
    except (KeyError, ValueError) as error:
        raise ScenarioTimelineError(
            "time_step_hours is required for scenario timeline metadata"
        ) from error
    if not math.isfinite(time_step_hours) or time_step_hours <= 0:
        raise ScenarioTimelineError("time_step_hours must be finite and positive")

    return ScenarioTimelineMetadata(
        series_start_utc=_parse_utc_datetime(values[SERIES_START_KEY]),
        time_step_hours=time_step_hours,
    )
