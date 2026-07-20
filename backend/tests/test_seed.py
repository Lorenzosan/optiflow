import csv
from pathlib import Path

from backend.app.seed import SEEDED_SCENARIOS


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def read_scenario_name(path: Path) -> str:
    with path.open(newline="", encoding="utf-8") as input_file:
        values = dict(csv.reader(input_file))
    return values["scenario_name"]


def test_seeded_scenario_files_exist_and_names_match() -> None:
    names = [scenario["name"] for scenario in SEEDED_SCENARIOS]
    assert len(names) == len(set(names))

    for scenario in SEEDED_SCENARIOS:
        scenario_path = REPOSITORY_ROOT / scenario["scenario_path"]
        prices_path = REPOSITORY_ROOT / scenario["prices_path"]
        inflows_path = REPOSITORY_ROOT / scenario["inflows_path"]

        assert scenario_path.is_file()
        assert prices_path.is_file()
        assert inflows_path.is_file()
        assert read_scenario_name(scenario_path) == scenario["name"]


def test_multistep_inflow_pulse_is_registered_as_bundled() -> None:
    multistep = next(
        scenario
        for scenario in SEEDED_SCENARIOS
        if scenario["name"] == "multistep_inflow_pulse"
    )

    assert multistep == {
        "name": "multistep_inflow_pulse",
        "description": (
            "Short hourly case with stepped prices and a three-hour "
            "50 MW hydraulic inflow pulse."
        ),
        "scenario_path": "examples/multistep/scenario.csv",
        "prices_path": "examples/multistep/prices.csv",
        "inflows_path": "examples/multistep/inflows.csv",
    }
