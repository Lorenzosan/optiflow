from pathlib import Path

import pytest

from backend.app.main import scenario_response
from backend.app.models import Scenario
from backend.app.scenario_metadata import ScenarioReportingError, load_scenario_reporting


def write_scenario(path: Path, extra_rows: str = "") -> None:
    path.write_text(
        "key,value\n"
        "scenario_name,test\n"
        "time_step_hours,1\n"
        f"{extra_rows}",
        encoding="utf-8",
    )


def test_load_scenario_reporting_returns_complete_metadata(tmp_path: Path) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(
        path,
        "market_start_utc,2026-12-31T23:00:00Z\n"
        "market_timezone,Europe/Zurich\n"
        "peak_start_hour,9\n"
        "peak_end_hour,20\n",
    )

    metadata = load_scenario_reporting(path)

    assert metadata is not None
    assert metadata.market_start_utc.isoformat() == "2026-12-31T23:00:00+00:00"
    assert metadata.market_timezone == "Europe/Zurich"
    assert metadata.peak_start_hour == 9
    assert metadata.peak_end_hour == 20
    assert metadata.time_step_hours == 1


def test_load_scenario_reporting_is_optional_for_legacy_scenarios(tmp_path: Path) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path)

    assert load_scenario_reporting(path) is None


@pytest.mark.parametrize(
    "extra_rows, message",
    [
        ("market_start_utc,2027-01-01T00:00:00Z\n", "incomplete"),
        (
            "market_start_utc,2027-01-01T00:00:00+01:00\n"
            "market_timezone,Europe/Zurich\n"
            "peak_start_hour,9\n"
            "peak_end_hour,20\n",
            "must use UTC",
        ),
        (
            "market_start_utc,2027-01-01T00:00:00Z\n"
            "market_timezone,not-a-timezone\n"
            "peak_start_hour,9\n"
            "peak_end_hour,20\n",
            "IANA-style timezone",
        ),
        (
            "market_start_utc,2027-01-01T00:00:00Z\n"
            "market_timezone,UTC\n"
            "peak_start_hour,20\n"
            "peak_end_hour,9\n",
            "earlier than",
        ),
    ],
)
def test_load_scenario_reporting_rejects_invalid_metadata(
    tmp_path: Path,
    extra_rows: str,
    message: str,
) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path, extra_rows)

    with pytest.raises(ScenarioReportingError, match=message):
        load_scenario_reporting(path)


def test_scenario_response_exposes_reporting_metadata(tmp_path: Path) -> None:
    scenario_dir = tmp_path / "scenario"
    scenario_dir.mkdir()
    write_scenario(
        scenario_dir / "scenario.csv",
        "market_start_utc,2027-01-01T00:00:00Z\n"
        "market_timezone,UTC\n"
        "peak_start_hour,9\n"
        "peak_end_hour,20\n",
    )
    (scenario_dir / "prices.csv").write_text("time_index,price\n0,1\n", encoding="utf-8")
    (scenario_dir / "inflows.csv").write_text(
        "time_index,natural_inflow\n0,0\n", encoding="utf-8"
    )
    scenario = Scenario(
        id=7,
        name="reporting_case",
        description="Reporting case",
        scenario_path="scenario/scenario.csv",
        prices_path="scenario/prices.csv",
        inflows_path="scenario/inflows.csv",
    )

    response = scenario_response(tmp_path, scenario)

    assert response.available is True
    assert response.reporting is not None
    assert response.reporting.market_timezone == "UTC"
    assert response.reporting.time_step_hours == 1
