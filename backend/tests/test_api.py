from collections.abc import Generator
from datetime import datetime, timedelta
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
from backend.app.scenario_uploads import ScenarioUploadError


TestSessionFactory = sessionmaker[Session]
ApiFixture = tuple[TestClient, TestSessionFactory, Scenario, Path, Path]


def sample_summary() -> RunSummaryData:
    return RunSummaryData(
        net_operating_cashflow=1234.5,
        export_energy_mwh=456.0,
        import_energy_mwh=78.0,
        final_reservoir_volume=52.5,
        solve_seconds=1.25,
        simulation_seconds=0.15,
        turbine_steps=10,
        pump_steps=3,
        spill_steps=1,
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
    examples_dir = root / "examples"
    examples_dir.mkdir()
    (examples_dir / "scenario.csv").write_text(
        "key,value\nscenario_name,test_scenario\ntime_step_hours,1\n",
        encoding="utf-8",
    )
    (examples_dir / "prices.csv").write_text(
        "timestamp_utc,price\n2027-01-01T00:00:00Z,10\n2027-01-01T01:00:00Z,20\n",
        encoding="utf-8",
    )
    (examples_dir / "inflows.csv").write_text(
        "timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,0\n2027-01-01T01:00:00Z,1\n",
        encoding="utf-8",
    )
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
    monkeypatch.setenv("OPTIFLOW_SCENARIO_STORAGE_DIR", str(root / "data" / "scenarios"))

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
    started_at: datetime | None = None,
) -> int:
    with testing_session() as db:
        run = OptimizationRun(
            scenario_id=scenario_id,
            status=run_status,
            output_dispatch_path=output_dispatch_path,
        )
        if started_at is not None:
            run.started_at = started_at
        db.add(run)
        db.commit()
        db.refresh(run)
        return run.id


def scenario_upload_files(
    *,
    name: str = "custom_case",
    scenario_filename: str = "scenario.csv",
) -> dict[str, tuple[str, bytes, str]]:
    return {
        "scenario": (
            scenario_filename,
            f"key,value\nscenario_name,{name}\ntime_step_hours,1\n".encode(),
            "text/csv",
        ),
        "prices": (
            "prices.csv",
            b"timestamp_utc,price\n2027-01-01T00:00:00Z,10\n2027-01-01T01:00:00Z,20\n",
            "text/csv",
        ),
        "inflows": (
            "inflows.csv",
            b"timestamp_utc,natural_inflow\n2027-01-01T00:00:00Z,0\n2027-01-01T01:00:00Z,1\n",
            "text/csv",
        ),
    }


def test_get_scenario_inputs_returns_bundled_files(api: ApiFixture) -> None:
    client, _, scenario, _, _ = api

    response = client.get(f"/scenarios/{scenario.id}/inputs")

    assert response.status_code == 200
    payload = response.json()
    assert payload == {
        "id": scenario.id,
        "name": "test_scenario",
        "description": "Test scenario",
        "editable": False,
        "scenario_csv": "key,value\nscenario_name,test_scenario\ntime_step_hours,1\n",
        "prices_csv": (
            "timestamp_utc,price\n"
            "2027-01-01T00:00:00Z,10\n"
            "2027-01-01T01:00:00Z,20\n"
        ),
        "inflows_csv": (
            "timestamp_utc,natural_inflow\n"
            "2027-01-01T00:00:00Z,0\n"
            "2027-01-01T01:00:00Z,1\n"
        ),
    }

def test_get_scenario_inputs_rejects_unmanaged_paths(api: ApiFixture, tmp_path: Path) -> None:
    client, testing_session, scenario, _, _ = api
    outside = tmp_path / "outside.csv"
    outside.write_text("private", encoding="utf-8")
    with testing_session() as db:
        persisted = db.get(Scenario, scenario.id)
        assert persisted is not None
        persisted.scenario_path = str(outside)
        db.commit()

    response = client.get(f"/scenarios/{scenario.id}/inputs")

    assert response.status_code == 409
    assert "outside managed storage" in response.json()["detail"]

def test_get_scenario_inputs_returns_not_found(api: ApiFixture) -> None:
    client, _, _, _, _ = api
    assert client.get("/scenarios/999/inputs").status_code == 404


def test_create_scenario_validates_stores_and_persists_uploads(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, _, root, _ = api

    def fake_validate(
        selected_root: Path,
        scenario_path: Path,
        prices_path: Path,
        inflows_path: Path,
    ) -> None:
        assert selected_root == root
        assert scenario_path.read_text() == (
            "key,value\nscenario_name,custom_case\ntime_step_hours,1\n"
        )
        assert prices_path.read_text().startswith("timestamp_utc,price")
        assert inflows_path.read_text().startswith("timestamp_utc,natural_inflow")

    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", fake_validate)
    response = client.post(
        "/scenarios",
        data={"description": "Custom uploaded scenario"},
        files=scenario_upload_files(),
    )
    assert response.status_code == 201
    payload = response.json()
    assert payload["name"] == "custom_case"
    assert payload["description"] == "Custom uploaded scenario"
    assert payload["available"] is True
    assert payload["editable"] is True
    assert payload["time_step_hours"] == 1

    inputs_response = client.get(f"/scenarios/{payload['id']}/inputs")
    assert inputs_response.status_code == 200
    assert inputs_response.json()["editable"] is True
    assert "scenario_name,custom_case" in inputs_response.json()["scenario_csv"]

    with testing_session() as db:
        persisted = db.get(Scenario, payload["id"])
        assert persisted is not None
        assert persisted.name == "custom_case"
        assert "scenario_name,custom_case\n" in (root / persisted.scenario_path).read_text()


@pytest.mark.parametrize("form_data", [{"description": "   "}, {}])
def test_create_scenario_allows_empty_description(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
    form_data: dict[str, str],
) -> None:
    client, testing_session, _, _, _ = api
    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", lambda *_: None)

    response = client.post(
        "/scenarios",
        data=form_data,
        files=scenario_upload_files(name="empty_description"),
    )

    assert response.status_code == 201
    payload = response.json()
    assert payload["description"] == ""
    with testing_session() as db:
        persisted = db.get(Scenario, payload["id"])
        assert persisted is not None
        assert persisted.description == ""


def test_create_scenario_rejects_duplicate_name_without_overwrite(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, _, _, _ = api
    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", lambda *_: None)
    first = client.post(
        "/scenarios",
        data={"description": "First"},
        files=scenario_upload_files(),
    )
    second = client.post(
        "/scenarios",
        data={"description": "Second"},
        files=scenario_upload_files(),
    )
    assert first.status_code == 201
    assert second.status_code == 409
    with testing_session() as db:
        assert db.query(Scenario).filter(Scenario.name == "custom_case").count() == 1


def test_create_scenario_overwrites_custom_inputs_and_deletes_prior_runs(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, _, root, output_dir = api
    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", lambda *_: None)
    first = client.post(
        "/scenarios",
        data={"description": "First"},
        files=scenario_upload_files(),
    )
    assert first.status_code == 201
    scenario_id = first.json()["id"]

    with testing_session() as db:
        original = db.get(Scenario, scenario_id)
        assert original is not None
        original_directory = (root / original.scenario_path).parent

    artifact = output_dir / "old-custom-dispatch.csv"
    artifact.write_text("old dispatch")
    run_id = add_run(
        testing_session,
        scenario_id,
        run_status="succeeded",
        output_dispatch_path=artifact.relative_to(root).as_posix(),
    )

    replaced = client.post(
        "/scenarios",
        data={"description": "   ", "overwrite": "true"},
        files=scenario_upload_files(),
    )
    assert replaced.status_code == 201
    payload = replaced.json()
    assert payload["id"] == scenario_id
    assert payload["description"] == ""

    with testing_session() as db:
        persisted = db.get(Scenario, scenario_id)
        assert persisted is not None
        assert (root / persisted.scenario_path).parent != original_directory
        assert db.get(OptimizationRun, run_id) is None
        assert db.query(Scenario).filter(Scenario.name == "custom_case").count() == 1

    assert not original_directory.exists()
    assert not artifact.exists()


def test_create_scenario_does_not_overwrite_bundled_scenario(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, scenario, _, _ = api
    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", lambda *_: None)
    response = client.post(
        "/scenarios",
        data={"description": "Replacement", "overwrite": "true"},
        files=scenario_upload_files(name=scenario.name),
    )
    assert response.status_code == 409
    assert response.json()["detail"] == "Bundled scenarios cannot be overwritten"
    with testing_session() as db:
        persisted = db.get(Scenario, scenario.id)
        assert persisted is not None
        assert persisted.description == "Test scenario"
        assert persisted.scenario_path == "examples/scenario.csv"


def test_create_scenario_rejects_invalid_inputs(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, _, _, _ = api

    def reject_inputs(*_args: object) -> None:
        raise ScenarioUploadError("terminal bounds are invalid")

    monkeypatch.setattr("backend.app.scenario_uploads.validate_scenario_inputs", reject_inputs)
    response = client.post(
        "/scenarios",
        data={"description": "Invalid"},
        files=scenario_upload_files(name="invalid_case"),
    )
    assert response.status_code == 422
    assert response.json()["detail"]["error"] == "terminal bounds are invalid"
    with testing_session() as db:
        assert db.query(Scenario).filter(Scenario.name == "invalid_case").count() == 0


def test_list_runs_returns_newest_first(api: ApiFixture) -> None:
    client, testing_session, scenario, _, _ = api
    base_time = datetime(2026, 1, 1, 12, 0, 0)
    run_ids = [
        add_run(
            testing_session,
            scenario.id,
            run_status=run_status,
            started_at=base_time + timedelta(minutes=index),
        )
        for index, run_status in enumerate(["failed", "succeeded", "running", "succeeded"])
    ]
    response = client.get("/runs", params={"limit": 2, "offset": 1})
    assert response.status_code == 200
    payload = response.json()
    assert payload["total"] == 4
    assert [item["id"] for item in payload["items"]] == [run_ids[2], run_ids[1]]
    assert all(item["started_at"].endswith("Z") for item in payload["items"])


@pytest.mark.parametrize(
    "query",
    ["limit=0", "limit=101", "offset=-1", "scenario_id=0", "status=unknown"],
)
def test_list_runs_rejects_invalid_query(api: ApiFixture, query: str) -> None:
    client, _, _, _, _ = api
    assert client.get(f"/runs?{query}").status_code == 422


def test_create_run_persists_unexpected_failure(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, scenario, _, _ = api

    def failing_run_solver(_root: Path, _scenario: Scenario, _run_id: int) -> SolverResult:
        raise RuntimeError("internal diagnostic")

    monkeypatch.setattr("backend.app.main.run_solver", failing_run_solver)
    response = client.post("/runs", json={"scenario_id": scenario.id})
    assert response.status_code == 500
    run_id = response.json()["detail"]["run_id"]
    with testing_session() as db:
        persisted = db.get(OptimizationRun, run_id)
        assert persisted is not None
        assert persisted.status == "failed"
        assert persisted.completed_at is not None


def test_create_run_persists_success(
    api: ApiFixture,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    client, testing_session, scenario, _, _ = api

    def fake_run_solver(_root: Path, selected: Scenario, run_id: int) -> SolverResult:
        assert selected.id == scenario.id
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
    assert payload["started_at"].endswith("Z")
    assert payload["completed_at"].endswith("Z")
    assert payload["summary"] == {
        "net_operating_cashflow": 1234.5,
        "export_energy_mwh": 456.0,
        "import_energy_mwh": 78.0,
        "final_reservoir_volume": 52.5,
        "solve_seconds": 1.25,
        "simulation_seconds": 0.15,
        "turbine_steps": 10,
        "pump_steps": 3,
        "spill_steps": 1,
        "wait_steps": 2,
    }
    with testing_session() as db:
        persisted = db.get(OptimizationRun, payload["id"])
        assert persisted is not None
        assert persisted.started_at.tzinfo is None
        assert persisted.completed_at is not None
        assert persisted.completed_at.tzinfo is None
        assert persisted.summary is not None
        assert persisted.summary.final_reservoir_volume == 52.5


def test_dispatch_download_returns_csv(api: ApiFixture) -> None:
    client, testing_session, scenario, root, output_dir = api
    artifact_path = output_dir / "run_000001_dispatch.csv"
    artifact_contents = "time_index,net_power\n0,12.5\n"
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


def test_dispatch_download_rejects_path_outside_output_directory(api: ApiFixture) -> None:
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
