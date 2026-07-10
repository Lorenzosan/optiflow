from pathlib import Path
import os
import subprocess
import sys

from sqlalchemy import create_engine, inspect, text

from backend.app.models import Base


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


def test_initial_migration_upgrades_and_downgrades_sqlite(tmp_path: Path) -> None:
    database_path = tmp_path / "migration-test.db"
    database_url = f"sqlite:///{database_path}"

    run_alembic(database_url, "upgrade", "head")
    run_alembic(database_url, "check")

    engine = create_engine(database_url)
    inspector = inspect(engine)
    assert set(inspector.get_table_names()) == {
        "alembic_version",
        "optimization_runs",
        "scenarios",
    }
    assert {column["name"] for column in inspector.get_columns("scenarios")} == {
        "id",
        "name",
        "description",
        "scenario_path",
        "prices_path",
        "inflows_path",
        "created_at",
    }
    assert {column["name"] for column in inspector.get_columns("optimization_runs")} == {
        "id",
        "scenario_id",
        "status",
        "started_at",
        "completed_at",
        "output_dispatch_path",
        "error_message",
    }
    engine.dispose()

    run_alembic(database_url, "downgrade", "base")

    engine = create_engine(database_url)
    assert inspect(engine).get_table_names() == ["alembic_version"]
    engine.dispose()


def test_existing_orm_schema_can_be_adopted(tmp_path: Path) -> None:
    database_path = tmp_path / "existing-schema.db"
    database_url = f"sqlite:///{database_path}"

    engine = create_engine(database_url)
    Base.metadata.create_all(engine)
    engine.dispose()

    run_alembic(database_url, "stamp", "head")
    run_alembic(database_url, "check")

    engine = create_engine(database_url)
    with engine.connect() as connection:
        revision = connection.execute(
            text("SELECT version_num FROM alembic_version")
        ).scalar_one()
    assert revision == "20260710_0001"
    engine.dispose()
