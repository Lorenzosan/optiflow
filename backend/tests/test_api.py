from collections.abc import Generator
from pathlib import Path

import pytest
from fastapi.testclient import TestClient
from sqlalchemy import create_engine
from sqlalchemy.orm import Session, sessionmaker
from sqlalchemy.pool import StaticPool

from backend.app.database import get_db
from backend.app.main import app
from backend.app.models import Base, OptimizationRun, Scenario
from backend.app.runner import RunSummaryData, SolverResult


TestSessionFactory = sessionmaker[Session]
ApiFixture = tuple[TestClient, TestSessionFactory, Scenario, Path, Path]


def sample_summary() -> RunSummaryData:
    return RunSummaryData(
        cumulative_profit=1234.5,
        export_energy_mwh=456.0,
        import_energy_mwh=78.0,
        final_reservoir_volume=52.5,
        final_battery_soc=4.0,
        solve_seconds=1.25,
        simulation_seconds=0.15,
        turbine_steps=10,
        pump_steps=3,
        spill_steps=1,
        battery_charge_steps=4,
        battery_discharge_steps=5,
        wait_steps=2,
    )


@pytest.fixture
def api(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
) -> Generator[ApiFixture, None, None]:
    root = tmp_path / "repository"
    output_dir = root / "build" / "api-runs"
    output_dir.mkdir(parents=True)

    engine = create_engine(
        "sqlite://",
        connect_args={"check_same_thread": False},
        poolclass=StaticPool,
    )
    testing_session = sessionmaker(bind=engine, expire_on_commit=False)
    Base.metadata.create_all(engine)

    with testing_session() as db:
        scenario = Scenario(
            name="test_scenario",
            description="Test scenario",
            scenario_path="examples/scenario.csv",
            prices_path="examples/prices.csv",
            inflows_path="examples/inflows.csv",
        )
        db.add(scenario)
        db.commit()
        db.refresh(scenario)
        scenario_id = scenario.id

    def override_get_db() -> Generator[Session, None, None]:
        with testing_session() as db:
            yield db

    app.dependency_overrides[get_db] = override_get_db
    monkeypatch.setenv("OPTIFLOW_REPO_ROOT", str(root))
    monkeypatch.setenv("OPTIFLOW_RUN_OUTPUT_DIR", str(output_dir))

    with testing_session() as db:
        scenario = db.get(Scenario, scenario_id)
        assert scenario is not None
        db.expunge(scenario)

    client = TestClient(app)
    try:
        yield client, testing_session, scenario, root, output_dir
    finally:
        client.close()
        app.dependency_overrides.clear()
        Base.metadata.drop_all(engine)
        engine.dispose()


def add_run(
    testing_session: TestSessionFactory,
    scenario_id: int,
    *,
    run_status: str,
    output_dispatch_path: str | None = None,
) -> int:
    with testing_session() as db:
        run = OptimizationRun(
            scenario_id=scenario_id,
            status=run_status,
            output_dispatch_path=output_dispatch_path,
        )
        db.add(run)
        db.commit()
        db.refresh(run)
        return run.id


def test_create_run_rejects_unknown_scenario(api: ApiFixture) -> None:
    client, _, _, _, _ = api

    response = client.post("/runs", json={"scenario_id": 999})

    assert response.status_code == 404
    assert response.json() == {"detail": "Scenario not found"}


def test_create_run_persists_unexpected_solver_failure(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, scenario, _, _ = api

    def failing_run_solver(_root: Path, _scenario: Scenario, _run_id: int) -> SolverResult:
        raise RuntimeError("internal diagnostic that must not reach the API")

    monkeypatch.setattr("backend.app.main.run_solver", failing_run_solver)

    response = client.post("/runs", json={"scenario_id": scenario.id})

    assert response.status_code == 500
    detail = response.json()["detail"]
    assert detail["message"] == "Optimization run failed unexpectedly"

    with testing_session() as db:
        persisted = db.get(OptimizationRun, detail["run_id"])
        assert persisted is not None
        assert persisted.status == "failed"
        assert persisted.completed_at is not None
        assert persisted.output_dispatch_path is None
        assert (
            persisted.error_message
            == "Unexpected solver execution error. See service logs for details."
        )


def test_create_run_persists_success(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, scenario, _, _ = api

    def fake_run_solver(_root: Path, selected_scenario: Scenario, run_id: int) -> SolverResult:
        assert selected_scenario.id == scenario.id
        return SolverResult(
            status="succeeded",
            output_dispatch_path=f"build/api-runs/run_{run_id:06d}_dispatch.csv",
            error_message=None,
            summary=sample_summary(),
        )

    monkeypatch.setattr("backend.app.main.run_solver", fake_run_solver)

    response = client.post("/runs", json={"scenario_id": scenario.id})

    assert response.status_code == 201
    payload = response.json()
    assert payload["scenario_id"] == scenario.id
    assert payload["scenario_name"] == scenario.name
    assert payload["status"] == "succeeded"
    assert payload["output_dispatch_path"] == f"build/api-runs/run_{payload['id']:06d}_dispatch.csv"
    assert payload["summary"] == {
        "cumulative_profit": 1234.5,
        "export_energy_mwh": 456.0,
        "import_energy_mwh": 78.0,
        "final_reservoir_volume": 52.5,
        "final_battery_soc": 4.0,
        "solve_seconds": 1.25,
        "simulation_seconds": 0.15,
        "turbine_steps": 10,
        "pump_steps": 3,
        "spill_steps": 1,
        "battery_charge_steps": 4,
        "battery_discharge_steps": 5,
        "wait_steps": 2,
    }

    with testing_session() as db:
        persisted = db.get(OptimizationRun, payload["id"])
        assert persisted is not None
        assert persisted.status == "succeeded"
        assert persisted.completed_at is not None
        assert persisted.summary is not None
        assert persisted.summary.cumulative_profit == 1234.5
        assert persisted.summary.final_reservoir_volume == 52.5
        assert persisted.summary.wait_steps == 2


@pytest.mark.parametrize("path", ["/runs/999", "/runs/999/dispatch.csv"])
def test_run_endpoints_reject_unknown_id(
    api: ApiFixture,
    path: str,
) -> None:
    client, _, _, _, _ = api

    response = client.get(path)

    assert response.status_code == 404
    assert response.json() == {"detail": "Run not found"}


def test_dispatch_download_rejects_failed_run(api: ApiFixture) -> None:
    client, testing_session, scenario, _, _ = api
    run_id = add_run(testing_session, scenario.id, run_status="failed")

    response = client.get(f"/runs/{run_id}/dispatch.csv")

    assert response.status_code == 409
    assert response.json() == {"detail": "Dispatch is available only for succeeded runs"}


def test_dispatch_download_returns_csv(api: ApiFixture) -> None:
    client, testing_session, scenario, root, output_dir = api
    artifact_path = output_dir / "run_000001_dispatch.csv"
    artifact_contents = "time,net_power_mw\n0,12.5\n"
    artifact_path.write_text(artifact_contents)
    run_id = add_run(
        testing_session,
        scenario.id,
        run_status="succeeded",
        output_dispatch_path=artifact_path.relative_to(root).as_posix(),
    )

    response = client.get(f"/runs/{run_id}/dispatch.csv")

    assert response.status_code == 200
    assert response.text == artifact_contents
    assert response.headers["content-type"].startswith("text/csv")
    assert (
        'filename="run_000001_dispatch.csv"'
        in response.headers["content-disposition"]
    )


def test_dispatch_download_rejects_path_outside_output_directory(
    api: ApiFixture,
) -> None:
    client, testing_session, scenario, root, _ = api
    outside_path = root / "private.csv"
    outside_path.write_text("secret\n")
    run_id = add_run(
        testing_session,
        scenario.id,
        run_status="succeeded",
        output_dispatch_path=outside_path.relative_to(root).as_posix(),
    )

    response = client.get(f"/runs/{run_id}/dispatch.csv")

    assert response.status_code == 500
    assert response.json() == {"detail": "Dispatch artifact path is invalid"}
