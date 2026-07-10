from pathlib import Path

import pytest

from backend.app.main import scenario_response
from backend.app.models import Scenario
from backend.app.scenario_parameters import ScenarioParameterError, load_time_step_hours


def write_scenario(path: Path, time_step: str = "1") -> None:
    path.write_text(
        "key,value\n"
        "scenario_name,test\n"
        f"time_step_hours,{time_step}\n",
        encoding="utf-8",
    )


def test_load_time_step_hours_returns_positive_value(tmp_path: Path) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path, "0.5")

    assert load_time_step_hours(path) == 0.5


@pytest.mark.parametrize("value", ["", "zero", "0", "-1", "nan", "inf"])
def test_load_time_step_hours_rejects_invalid_values(tmp_path: Path, value: str) -> None:
    path = tmp_path / "scenario.csv"
    write_scenario(path, value)

    with pytest.raises(ScenarioParameterError):
        load_time_step_hours(path)


def test_scenario_response_exposes_optimizer_time_step(tmp_path: Path) -> None:
    scenario_dir = tmp_path / "scenario"
    scenario_dir.mkdir()
    write_scenario(scenario_dir / "scenario.csv", "2")
    (scenario_dir / "prices.csv").write_text(
        "timestamp_utc,price\n2027-01-01T00:00:00Z,1\n",
        encoding="utf-8",
    )
    (scenario_dir / "inflows.csv").write_text(
        "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,0\n",
        encoding="utf-8",
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
    assert response.time_step_hours == 2
