from __future__ import annotations

import csv
import math
from pathlib import Path


class ScenarioParameterError(ValueError):
    """A required scenario parameter cannot be read safely."""


def load_time_step_hours(path: Path) -> float:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.reader(handle)
            if next(reader, None) != ["key", "value"]:
                raise ScenarioParameterError("scenario file must use the header key,value")

            values: dict[str, str] = {}
            for row_number, row in enumerate(reader, start=2):
                if len(row) != 2:
                    raise ScenarioParameterError(
                        f"scenario row {row_number} must contain exactly two columns"
                    )
                key = row[0].strip()
                if not key:
                    raise ScenarioParameterError(f"scenario row {row_number} has an empty key")
                if key in values:
                    raise ScenarioParameterError(f"scenario contains duplicate key {key}")
                values[key] = row[1].strip()
    except (OSError, UnicodeError) as error:
        raise ScenarioParameterError("scenario file must be readable UTF-8 CSV") from error

    try:
        time_step_hours = float(values["time_step_hours"])
    except KeyError as error:
        raise ScenarioParameterError("scenario is missing time_step_hours") from error
    except ValueError as error:
        raise ScenarioParameterError("time_step_hours must be numeric") from error

    if not math.isfinite(time_step_hours) or time_step_hours <= 0:
        raise ScenarioParameterError("time_step_hours must be finite and positive")
    return time_step_hours
