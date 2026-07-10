from pathlib import Path

import pytest

from backend.app.main import scenario_response
from backend.app.models import Scenario
from backend.app.scenario_timeline import ScenarioTimelineError, load_scenario_timeline


def write_scenario(path: Path, extra_rows: str = "") -> None:
    path.write_text(
        "key,value\n"
        "scenario_name,test\n"
        "time_step_hours,1\n"
        f"{extra_rows}",
        encoding="utf-8",
    )


def test_load_scenario_timeline_returns_series_start_and_step(tmp_path: Path) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path, "series_start_utc,2026-12-31T23:00:00Z\n")

    metadata = load_scenario_timeline(path)

    assert metadata.series_start_utc.isoformat() == "2026-12-31T23:00:00+00:00"
    assert metadata.time_step_hours == 1


@pytest.mark.parametrize(
    "extra_rows, message",
    [
        ("", "missing series_start_utc"),
        ("series_start_utc,2027-01-01T00:00:00+01:00\n", "must use UTC"),
        ("series_start_utc,not-a-date\n", "ISO-8601"),
    ],
)
def test_load_scenario_timeline_rejects_invalid_metadata(
    tmp_path: Path,
    extra_rows: str,
    message: str,
) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path, extra_rows)

    with pytest.raises(ScenarioTimelineError, match=message):
        load_scenario_timeline(path)


def test_scenario_response_exposes_timeline_metadata(tmp_path: Path) -> None:
    scenario_dir = tmp_path / "scenario"
    scenario_dir.mkdir()
    write_scenario(
        scenario_dir / "scenario.csv",
        "series_start_utc,2027-01-01T00:00:00Z\n",
    )
    (scenario_dir / "prices.csv").write_text("time_index,price\n0,1\n", encoding="utf-8")
    (scenario_dir / "inflows.csv").write_text(
        "time_index,natural_inflow\n0,0\n", encoding="utf-8"
    )
    scenario = Scenario(
        id=7,
        name="timeline_case",
        description="Timeline case",
        scenario_path="scenario/scenario.csv",
        prices_path="scenario/prices.csv",
        inflows_path="scenario/inflows.csv",
    )

    response = scenario_response(tmp_path, scenario)

    assert response.available is True
    assert response.timeline is not None
    assert response.timeline.series_start_utc.isoformat() == "2027-01-01T00:00:00+00:00"
    assert response.timeline.time_step_hours == 1
