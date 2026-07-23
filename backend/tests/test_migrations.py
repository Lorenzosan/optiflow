from pathlib import Path
import os
import subprocess
import sys

from sqlalchemy import create_engine, inspect, text


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def run_alembic(database_url: str, *arguments: str) -> None:
    environment = os.environ.copy()
    environment["OPTIFLOW_DATABASE_URL"] = database_url
    subprocess.run(
        [sys.executable, "-m", "alembic", *arguments],
        cwd=REPOSITORY_ROOT,
        env=environment,
        check=True,
        capture_output=True,
        text=True,
    )


def test_migrations_upgrade_and_downgrade_sqlite(tmp_path: Path) -> None:
    database_path = tmp_path / "migration-test.db"
    database_url = f"sqlite:///{database_path}"

    run_alembic(database_url, "upgrade", "head")
    run_alembic(database_url, "check")

    engine = create_engine(database_url)
    inspector = inspect(engine)
    assert set(inspector.get_table_names()) == {
        "alembic_version",
        "optimization_runs",
        "run_summaries",
        "scenarios",
    }
    assert {column["name"] for column in inspector.get_columns("run_summaries")} == {
        "run_id",
        "net_operating_cashflow",
        "export_energy_mwh",
        "import_energy_mwh",
        "final_reservoir_volume",
        "solve_seconds",
        "simulation_seconds",
        "turbine_steps",
        "pump_steps",
        "spill_steps",
        "wait_steps",
    }
    with engine.connect() as connection:
        revision = connection.execute(text("SELECT version_num FROM alembic_version")).scalar_one()
    assert revision == "20260710_0001"
    engine.dispose()

    run_alembic(database_url, "downgrade", "base")
    engine = create_engine(database_url)
    assert inspect(engine).get_table_names() == ["alembic_version"]
    engine.dispose()

