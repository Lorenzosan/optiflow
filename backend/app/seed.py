from sqlalchemy.orm import Session

from backend.app.models import Scenario


SEEDED_SCENARIOS: tuple[dict[str, str], ...] = (
    {
        "name": "multistep_inflow_pulse",
        "description": "Short hourly case with stepped prices and a three-hour 50 MW hydraulic inflow pulse.",
        "scenario_path": "examples/multistep/scenario.csv",
        "prices_path": "examples/multistep/prices.csv",
        "inflows_path": "examples/multistep/inflows.csv",
    },
    {
        "name": "synthetic_year",
        "description": "Yearly pumped-storage case with pumping, generation, spill, and terminal inventory requirements.",
        "scenario_path": "examples/yearly/scenario.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
    {
        "name": "synthetic_year_no_pumping",
        "description": "Yearly sensitivity where the station cannot pump water back into the upper reservoir.",
        "scenario_path": "examples/yearly/scenario_no_pumping.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
    {
        "name": "synthetic_year_high_operating_cost",
        "description": "Yearly sensitivity where hydraulic throughput has a high operating cost.",
        "scenario_path": "examples/yearly/scenario_high_operating_cost.csv",
        "prices_path": "examples/yearly/prices.csv",
        "inflows_path": "examples/yearly/inflows.csv",
    },
)


def seed_scenarios(db: Session) -> None:
    for scenario_data in SEEDED_SCENARIOS:
        scenario = db.query(Scenario).filter(Scenario.name == scenario_data["name"]).one_or_none()
        if scenario is None:
            db.add(Scenario(**scenario_data))
            continue
        scenario.description = scenario_data["description"]
        scenario.scenario_path = scenario_data["scenario_path"]
        scenario.prices_path = scenario_data["prices_path"]
        scenario.inflows_path = scenario_data["inflows_path"]
    db.commit()
