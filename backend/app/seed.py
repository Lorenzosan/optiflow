from sqlalchemy.orm import Session

from backend.app.models import Scenario


SEEDED_SCENARIOS: tuple[dict[str, str], ...] = (
    {
        "name": "synthetic_year",
        "description": "Yearly pumped-storage and battery case with economically usable battery cycling.",
        "scenario_path": "examples/yearly/scenario.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
    {
        "name": "synthetic_year_no_battery",
        "description": "Yearly case where the battery is physically unavailable.",
        "scenario_path": "examples/yearly/scenario_no_battery.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
    {
        "name": "synthetic_year_high_battery_degradation",
        "description": "Yearly case where the battery is available but economically unattractive to cycle.",
        "scenario_path": "examples/yearly/scenario_high_battery_degradation.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
)


def seed_scenarios(db: Session) -> None:
    for scenario_data in SEEDED_SCENARIOS:
        scenario = (
            db.query(Scenario)
            .filter(Scenario.name == scenario_data["name"])
            .one_or_none()
        )
        if scenario is None:
            db.add(Scenario(**scenario_data))
            continue

        scenario.description = scenario_data["description"]
        scenario.scenario_path = scenario_data["scenario_path"]
        scenario.prices_path = scenario_data["prices_path"]
        scenario.inflows_path = scenario_data["inflows_path"]

    db.commit()
